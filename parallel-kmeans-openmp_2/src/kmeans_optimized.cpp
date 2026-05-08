#include "kmeans_optimized.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

#if defined(_WIN32)
#include <malloc.h>
#endif

#include <omp.h>

namespace {

constexpr std::size_t CACHE_LINE_BYTES = 64;

template <typename T, std::size_t Alignment>
class AlignedAllocator {
public:
    using value_type = T;

    AlignedAllocator() noexcept = default;

    template <typename U>
    AlignedAllocator(const AlignedAllocator<U, Alignment> &) noexcept {}

    T *allocate(std::size_t n) {
        if (n == 0) {
            return nullptr;
        }
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::bad_array_new_length();
        }

        void *ptr = nullptr;
#if defined(_WIN32)
        ptr = _aligned_malloc(n * sizeof(T), Alignment);
        if (ptr == nullptr) {
            throw std::bad_alloc();
        }
#else
        if (posix_memalign(&ptr, Alignment, n * sizeof(T)) != 0) {
            throw std::bad_alloc();
        }
#endif
        return static_cast<T *>(ptr);
    }

    void deallocate(T *ptr, std::size_t) noexcept {
#if defined(_WIN32)
        _aligned_free(ptr);
#else
        std::free(ptr);
#endif
    }

    template <typename U>
    struct rebind {
        using other = AlignedAllocator<U, Alignment>;
    };
};

template <typename T1, std::size_t A1, typename T2, std::size_t A2>
bool operator==(const AlignedAllocator<T1, A1> &, const AlignedAllocator<T2, A2> &) noexcept {
    return A1 == A2;
}

template <typename T1, std::size_t A1, typename T2, std::size_t A2>
bool operator!=(const AlignedAllocator<T1, A1> &lhs, const AlignedAllocator<T2, A2> &rhs) noexcept {
    return !(lhs == rhs);
}

using AlignedDoubleVector = std::vector<double, AlignedAllocator<double, CACHE_LINE_BYTES>>;
using AlignedSizeVector = std::vector<std::size_t, AlignedAllocator<std::size_t, CACHE_LINE_BYTES>>;

struct alignas(CACHE_LINE_BYTES) ThreadLocalStats {
    AlignedDoubleVector sums;
    AlignedSizeVector counts;
};

struct NearestResult {
    int label;
    double best_distance;
    double second_distance;
    double best_distance_sq;
};

std::size_t padded_element_count(std::size_t elements, std::size_t element_size) {
    const std::size_t elements_per_line = std::max<std::size_t>(1, CACHE_LINE_BYTES / element_size);
    return ((elements + elements_per_line - 1) / elements_per_line) * elements_per_line;
}

std::vector<double> build_soa_fallback(const Dataset &data) {
    std::vector<double> soa(data.n * data.dim);
    for (std::size_t i = 0; i < data.n; ++i) {
        for (std::size_t d = 0; d < data.dim; ++d) {
            soa[d * data.n + i] = data.values[i * data.dim + d];
        }
    }
    return soa;
}

AlignedDoubleVector init_centroids_soa(const std::vector<double> &soa_values,
                                       std::size_t n,
                                       std::size_t dim,
                                       std::size_t k) {
    if (k > n) {
        throw std::invalid_argument("k 不能大于样本数量 n");
    }

    // 与 serial/parallel 一样使用前 k 个样本初始化，只是中心按 SoA 形式存储。
    AlignedDoubleVector centroids(k * dim);
    for (std::size_t d = 0; d < dim; ++d) {
        for (std::size_t c = 0; c < k; ++c) {
            centroids[d * k + c] = soa_values[d * n + c];
        }
    }
    return centroids;
}

double center_distance_sq(const AlignedDoubleVector &centroids,
                          std::size_t dim,
                          std::size_t k,
                          std::size_t a,
                          std::size_t b) {
    double sum = 0.0;
    for (std::size_t d = 0; d < dim; ++d) {
        const double diff = centroids[d * k + a] - centroids[d * k + b];
        sum += diff * diff;
    }
    return sum;
}

NearestResult nearest_two_centroids_soa(const std::vector<double> &soa_values,
                                        const AlignedDoubleVector &centroids,
                                        std::size_t n,
                                        std::size_t dim,
                                        std::size_t k,
                                        std::size_t sample,
                                        std::vector<double> &distance_buffer) {
    std::fill(distance_buffer.begin(), distance_buffer.end(), 0.0);

    // SoA: 固定一个维度时，中心数组 centroids[d * k + c] 连续，
    // 内层按 c 遍历，便于编译器对多个中心的距离累加做 SIMD 自动向量化。
    const long long k_ll = static_cast<long long>(k);
    for (std::size_t d = 0; d < dim; ++d) {
        const double x = soa_values[d * n + sample];
        const double *center_row = &centroids[d * k];
#pragma omp simd
        for (long long c_ll = 0; c_ll < k_ll; ++c_ll) {
            const std::size_t c = static_cast<std::size_t>(c_ll);
            const double diff = x - center_row[c];
            distance_buffer[c] += diff * diff;
        }
    }

    int best_label = 0;
    double best_sq = std::numeric_limits<double>::max();
    double second_sq = std::numeric_limits<double>::max();
    for (std::size_t c = 0; c < k; ++c) {
        const double dist_sq = distance_buffer[c];
        if (dist_sq < best_sq) {
            second_sq = best_sq;
            best_sq = dist_sq;
            best_label = static_cast<int>(c);
        } else if (dist_sq < second_sq) {
            second_sq = dist_sq;
        }
    }

    NearestResult result;
    result.label = best_label;
    result.best_distance = std::sqrt(best_sq);
    result.second_distance = std::sqrt(second_sq);
    result.best_distance_sq = best_sq;
    return result;
}

AlignedDoubleVector compute_half_min_center_distances(const AlignedDoubleVector &centroids,
                                                      std::size_t dim,
                                                      std::size_t k) {
    AlignedDoubleVector half_min(k, std::numeric_limits<double>::max());
    if (k == 1) {
        return half_min;
    }

    for (std::size_t a = 0; a < k; ++a) {
        double best_sq = std::numeric_limits<double>::max();
        for (std::size_t b = 0; b < k; ++b) {
            if (a == b) {
                continue;
            }
            best_sq = std::min(best_sq, center_distance_sq(centroids, dim, k, a, b));
        }
        half_min[a] = 0.5 * std::sqrt(best_sq);
    }
    return half_min;
}

std::vector<double> centroids_soa_to_aos(const AlignedDoubleVector &centroids_soa,
                                         std::size_t dim,
                                         std::size_t k) {
    std::vector<double> centroids_aos(k * dim);
    for (std::size_t c = 0; c < k; ++c) {
        for (std::size_t d = 0; d < dim; ++d) {
            centroids_aos[c * dim + d] = centroids_soa[d * k + c];
        }
    }
    return centroids_aos;
}

} // namespace

KMeansResult kmeans_optimized(const Dataset &data, const KMeansConfig &config) {
    if (config.k == 0 || config.max_iter <= 0 || config.threads <= 0) {
        throw std::invalid_argument("k、max_iter 和 threads 必须大于 0");
    }

    const int thread_count = std::max(1, config.threads);
    omp_set_dynamic(0);
    omp_set_num_threads(thread_count);

    std::vector<double> fallback_soa;
    const std::vector<double> *soa_ptr = &data.soa_values;
    if (data.soa_values.size() != data.n * data.dim) {
        fallback_soa = build_soa_fallback(data);
        soa_ptr = &fallback_soa;
    }
    const std::vector<double> &soa_values = *soa_ptr;

    const std::size_t n = data.n;
    const std::size_t dim = data.dim;
    const std::size_t k = config.k;

    AlignedDoubleVector centroids = init_centroids_soa(soa_values, n, dim, k);
    std::vector<int> labels(n, -1);
    std::vector<double> upper_bounds(n, 0.0); // 到当前所属中心的上界
    std::vector<double> lower_bounds(n, 0.0); // 到其它中心的下界

    // 初始精确分配，建立 Hamerly 所需的 upper/lower bound。
    const long long n_ll = static_cast<long long>(n);
    unsigned long long distance_calculations = 0;
    unsigned long long skipped_points = 0;
    unsigned long long pruning_checks = 0;

#pragma omp parallel
    {
        std::vector<double> distance_buffer(k, 0.0);
#pragma omp for schedule(static) reduction(+:distance_calculations)
        for (long long i_ll = 0; i_ll < n_ll; ++i_ll) {
            const std::size_t i = static_cast<std::size_t>(i_ll);
            const NearestResult nearest = nearest_two_centroids_soa(soa_values, centroids, n, dim, k, i, distance_buffer);
            distance_calculations += static_cast<unsigned long long>(k);
            labels[i] = nearest.label;
            upper_bounds[i] = nearest.best_distance;
            lower_bounds[i] = nearest.second_distance;
        }
    }

    const std::size_t raw_sum_size = k * dim;
    const std::size_t padded_sum_size = padded_element_count(raw_sum_size, sizeof(double));
    const std::size_t padded_count_size = padded_element_count(k, sizeof(std::size_t));
    std::vector<ThreadLocalStats> local_stats(static_cast<std::size_t>(thread_count));
    for (ThreadLocalStats &stats : local_stats) {
        stats.sums.resize(padded_sum_size);
        stats.counts.resize(padded_count_size);
    }

    int iterations = 0;
    for (int iter = 0; iter < config.max_iter; ++iter) {
        ++iterations;
        const AlignedDoubleVector half_min_distances = compute_half_min_center_distances(centroids, dim, k);

#pragma omp parallel
        {
            const int tid = omp_get_thread_num();
            ThreadLocalStats &stats = local_stats[static_cast<std::size_t>(tid)];
            std::fill(stats.sums.begin(), stats.sums.end(), 0.0);
            std::fill(stats.counts.begin(), stats.counts.end(), 0);
            std::vector<double> distance_buffer(k, 0.0);

#pragma omp for schedule(static) reduction(+:distance_calculations, skipped_points, pruning_checks)
            for (long long i_ll = 0; i_ll < n_ll; ++i_ll) {
                const std::size_t i = static_cast<std::size_t>(i_ll);
                int label = labels[i];
                const std::size_t label_index = static_cast<std::size_t>(label);
                const double hamerly_threshold = std::max(lower_bounds[i], half_min_distances[label_index]);

                ++pruning_checks;
                // Hamerly 剪枝：满足条件时跳过完整的样本-中心距离计算。
                if (upper_bounds[i] <= hamerly_threshold) {
                    ++skipped_points;
                } else {
                    const NearestResult nearest = nearest_two_centroids_soa(soa_values,
                                                                            centroids,
                                                                            n,
                                                                            dim,
                                                                            k,
                                                                            i,
                                                                            distance_buffer);
                    distance_calculations += static_cast<unsigned long long>(k);
                    label = nearest.label;
                    labels[i] = label;
                    upper_bounds[i] = nearest.best_distance;
                    lower_bounds[i] = nearest.second_distance;
                }

                const std::size_t c = static_cast<std::size_t>(labels[i]);
                ++stats.counts[c];
                for (std::size_t d = 0; d < dim; ++d) {
                    stats.sums[d * k + c] += soa_values[d * n + i];
                }
            }
        }

        AlignedDoubleVector sums(k * dim, 0.0);
        std::vector<std::size_t> counts(k, 0);
        for (const ThreadLocalStats &stats : local_stats) {
            for (std::size_t c = 0; c < k; ++c) {
                counts[c] += stats.counts[c];
            }
            for (std::size_t d = 0; d < dim; ++d) {
                for (std::size_t c = 0; c < k; ++c) {
                    sums[d * k + c] += stats.sums[d * k + c];
                }
            }
        }

        AlignedDoubleVector new_centroids = centroids;
        for (std::size_t c = 0; c < k; ++c) {
            if (counts[c] == 0) {
                continue; // 空簇保持旧中心。
            }
            for (std::size_t d = 0; d < dim; ++d) {
                new_centroids[d * k + c] = sums[d * k + c] / static_cast<double>(counts[c]);
            }
        }

        std::vector<double> movements(k, 0.0);
        double max_movement = 0.0;
        for (std::size_t c = 0; c < k; ++c) {
            double movement_sq = 0.0;
            for (std::size_t d = 0; d < dim; ++d) {
                const double diff = centroids[d * k + c] - new_centroids[d * k + c];
                movement_sq += diff * diff;
            }
            movements[c] = std::sqrt(movement_sq);
            max_movement = std::max(max_movement, movements[c]);
        }

        centroids.swap(new_centroids);

        // 中心移动后更新 Hamerly bound，为下一轮剪枝做准备。
#pragma omp parallel for schedule(static)
        for (long long i_ll = 0; i_ll < n_ll; ++i_ll) {
            const std::size_t i = static_cast<std::size_t>(i_ll);
            const std::size_t label = static_cast<std::size_t>(labels[i]);
            upper_bounds[i] += movements[label];
            lower_bounds[i] = std::max(0.0, lower_bounds[i] - max_movement);
        }

        if (max_movement <= config.tol) {
            break;
        }
    }

    double inertia = 0.0;
#pragma omp parallel reduction(+:inertia)
    {
        std::vector<double> distance_buffer(k, 0.0);
#pragma omp for schedule(static) reduction(+:distance_calculations)
        for (long long i_ll = 0; i_ll < n_ll; ++i_ll) {
            const std::size_t i = static_cast<std::size_t>(i_ll);
            const NearestResult nearest = nearest_two_centroids_soa(soa_values, centroids, n, dim, k, i, distance_buffer);
            distance_calculations += static_cast<unsigned long long>(k);
            labels[i] = nearest.label;
            inertia += nearest.best_distance_sq;
        }
    }

    KMeansResult result;
    result.iterations = iterations;
    result.inertia = inertia;
    result.distance_calculations = static_cast<std::uint64_t>(distance_calculations);
    result.skipped_points = static_cast<std::uint64_t>(skipped_points);
    result.pruning_rate = pruning_checks == 0
                              ? 0.0
                              : static_cast<double>(skipped_points) / static_cast<double>(pruning_checks);
    result.centroids = centroids_soa_to_aos(centroids, dim, k);
    result.labels = std::move(labels);
    return result;
}
