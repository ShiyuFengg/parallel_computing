#include "dataset.h"

#include <algorithm>
#include <random>
#include <stdexcept>

Dataset generate_random_dataset(std::size_t n, std::size_t dim, std::size_t k, std::uint32_t seed) {
    if (n == 0 || dim == 0 || k == 0) {
        throw std::invalid_argument("n、dim 和 k 必须大于 0");
    }

    Dataset dataset;
    dataset.n = n;
    dataset.dim = dim;
    dataset.values.resize(n * dim);
    dataset.soa_values.resize(n * dim);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> center_dist(-100.0, 100.0);
    std::normal_distribution<double> noise_dist(0.0, 5.0);
    std::uniform_int_distribution<std::size_t> cluster_dist(0, k - 1);

    std::vector<double> true_centers(k * dim);
    for (double &value : true_centers) {
        value = center_dist(rng);
    }

    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t cluster = cluster_dist(rng);
        for (std::size_t d = 0; d < dim; ++d) {
            const double value = true_centers[cluster * dim + d] + noise_dist(rng);
            dataset.values[i * dim + d] = value;
            dataset.soa_values[d * n + i] = value;
        }
    }

    return dataset;
}
