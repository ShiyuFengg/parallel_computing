#include "kmeans_serial.h"

#include "metrics.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace {

std::vector<double> init_centroids_from_samples(const Dataset &data, std::size_t k) {
    if (k > data.n) {
        throw std::invalid_argument("k 不能大于样本数量 n");
    }

    // 串行和并行版本都使用前 k 个样本作为初始中心。
    // 只要数据生成 seed 相同，初始中心就完全一致，便于公平比较运行时间。
    std::vector<double> centroids(k * data.dim);
    for (std::size_t c = 0; c < k; ++c) {
        for (std::size_t d = 0; d < data.dim; ++d) {
            centroids[c * data.dim + d] = data.values[c * data.dim + d];
        }
    }
    return centroids;
}

} // namespace

KMeansResult kmeans_serial(const Dataset &data, const KMeansConfig &config) {
    if (config.k == 0 || config.max_iter <= 0) {
        throw std::invalid_argument("k 必须大于 0，max_iter 必须大于 0");
    }

    std::vector<double> centroids = init_centroids_from_samples(data, config.k);
    std::vector<int> labels(data.n, -1);
    int iterations = 0;

    for (int iter = 0; iter < config.max_iter; ++iter) {
        ++iterations;

        std::vector<double> sums(config.k * data.dim, 0.0);
        std::vector<std::size_t> counts(config.k, 0);

        // 样本分配阶段：为每个样本寻找最近的聚类中心。
        for (std::size_t i = 0; i < data.n; ++i) {
            const int cluster = nearest_centroid(data, centroids, i, config.k);
            labels[i] = cluster;

            ++counts[static_cast<std::size_t>(cluster)];
            for (std::size_t d = 0; d < data.dim; ++d) {
                sums[static_cast<std::size_t>(cluster) * data.dim + d] += data.values[i * data.dim + d];
            }
        }

        // 聚类中心更新阶段：每个中心移动到所属样本的均值位置。
        std::vector<double> new_centroids = centroids;
        for (std::size_t c = 0; c < config.k; ++c) {
            if (counts[c] == 0) {
                continue; // 空簇保持原中心，避免除以 0。
            }
            for (std::size_t d = 0; d < data.dim; ++d) {
                new_centroids[c * data.dim + d] = sums[c * data.dim + d] / static_cast<double>(counts[c]);
            }
        }

        double max_center_shift = 0.0;
        for (std::size_t c = 0; c < config.k; ++c) {
            const double shift = std::sqrt(squared_distance(&centroids[c * data.dim],
                                                            &new_centroids[c * data.dim],
                                                            data.dim));
            max_center_shift = std::max(max_center_shift, shift);
        }

        centroids.swap(new_centroids);
        if (max_center_shift <= config.tol) {
            break;
        }
    }

    // 使用最终中心重新分配一次标签，使 SSE/inertia 与最终中心严格对应。
    for (std::size_t i = 0; i < data.n; ++i) {
        labels[i] = nearest_centroid(data, centroids, i, config.k);
    }

    KMeansResult result;
    result.iterations = iterations;
    result.inertia = compute_inertia(data, centroids, labels, config.k);
    result.centroids = std::move(centroids);
    result.labels = std::move(labels);
    return result;
}
