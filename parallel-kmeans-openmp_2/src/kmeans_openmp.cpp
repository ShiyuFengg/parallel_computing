#include "kmeans_openmp.h"

#include "metrics.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include <omp.h>

namespace {

std::vector<double> init_centroids_from_samples_parallel(const Dataset &data, std::size_t k) {
    if (k > data.n) {
        throw std::invalid_argument("k 不能大于样本数量 n");
    }

    // 与串行版本保持相同的确定性初始化策略，保证实验对比只反映实现方式差异。
    std::vector<double> centroids(k * data.dim);
    for (std::size_t c = 0; c < k; ++c) {
        for (std::size_t d = 0; d < data.dim; ++d) {
            centroids[c * data.dim + d] = data.values[c * data.dim + d];
        }
    }
    return centroids;
}

} // namespace

KMeansResult kmeans_openmp(const Dataset &data, const KMeansConfig &config) {
    if (config.k == 0 || config.max_iter <= 0 || config.threads <= 0) {
        throw std::invalid_argument("k、max_iter 和 threads 必须大于 0");
    }

    const int thread_count = std::max(1, config.threads);
    omp_set_dynamic(0);
    omp_set_num_threads(thread_count);

    std::vector<double> centroids = init_centroids_from_samples_parallel(data, config.k);
    std::vector<int> labels(data.n, -1);
    int iterations = 0;

    for (int iter = 0; iter < config.max_iter; ++iter) {
        ++iterations;

        // 每个线程拥有独立的局部 sum/count，避免多个线程同时写同一簇产生数据竞争。
        std::vector<double> local_sums(static_cast<std::size_t>(thread_count) * config.k * data.dim, 0.0);
        std::vector<std::size_t> local_counts(static_cast<std::size_t>(thread_count) * config.k, 0);

        const long long n_ll = static_cast<long long>(data.n);
#pragma omp parallel
        {
            const int tid = omp_get_thread_num();
            double *thread_sums = &local_sums[static_cast<std::size_t>(tid) * config.k * data.dim];
            std::size_t *thread_counts = &local_counts[static_cast<std::size_t>(tid) * config.k];

#pragma omp for schedule(static)
            for (long long i_ll = 0; i_ll < n_ll; ++i_ll) {
                const std::size_t i = static_cast<std::size_t>(i_ll);
                const int cluster = nearest_centroid(data, centroids, i, config.k);
                labels[i] = cluster;

                const std::size_t c = static_cast<std::size_t>(cluster);
                ++thread_counts[c];
                for (std::size_t d = 0; d < data.dim; ++d) {
                    thread_sums[c * data.dim + d] += data.values[i * data.dim + d];
                }
            }
        }

        // 合并所有线程的局部统计量，得到全局 sum/count。
        std::vector<double> sums(config.k * data.dim, 0.0);
        std::vector<std::size_t> counts(config.k, 0);
        for (int t = 0; t < thread_count; ++t) {
            const std::size_t base_sum = static_cast<std::size_t>(t) * config.k * data.dim;
            const std::size_t base_count = static_cast<std::size_t>(t) * config.k;
            for (std::size_t c = 0; c < config.k; ++c) {
                counts[c] += local_counts[base_count + c];
                for (std::size_t d = 0; d < data.dim; ++d) {
                    sums[c * data.dim + d] += local_sums[base_sum + c * data.dim + d];
                }
            }
        }

        std::vector<double> new_centroids = centroids;
        for (std::size_t c = 0; c < config.k; ++c) {
            if (counts[c] == 0) {
                continue; // 空簇保持原中心。
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

    double inertia = 0.0;
    const long long n_ll = static_cast<long long>(data.n);
#pragma omp parallel for schedule(static) reduction(+:inertia)
    for (long long i_ll = 0; i_ll < n_ll; ++i_ll) {
        const std::size_t i = static_cast<std::size_t>(i_ll);
        labels[i] = nearest_centroid(data, centroids, i, config.k);
        inertia += squared_distance(&data.values[i * data.dim],
                                    &centroids[static_cast<std::size_t>(labels[i]) * data.dim],
                                    data.dim);
    }

    KMeansResult result;
    result.iterations = iterations;
    result.inertia = inertia;
    result.centroids = std::move(centroids);
    result.labels = std::move(labels);
    return result;
}
