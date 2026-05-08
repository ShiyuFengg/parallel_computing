#ifndef METRICS_H
#define METRICS_H

#include "dataset.h"

#include <cstddef>
#include <limits>
#include <vector>

inline double squared_distance(const double *a, const double *b, std::size_t dim) {
    double sum = 0.0;
    for (std::size_t d = 0; d < dim; ++d) {
        const double diff = a[d] - b[d];
        sum += diff * diff;
    }
    return sum;
}

inline double compute_inertia(const Dataset &data,
                              const std::vector<double> &centroids,
                              const std::vector<int> &labels,
                              std::size_t k) {
    (void)k;
    double inertia = 0.0;
    for (std::size_t i = 0; i < data.n; ++i) {
        const int cluster = labels[i];
        inertia += squared_distance(&data.values[i * data.dim],
                                    &centroids[static_cast<std::size_t>(cluster) * data.dim],
                                    data.dim);
    }
    return inertia;
}

inline int nearest_centroid(const Dataset &data,
                            const std::vector<double> &centroids,
                            std::size_t sample_index,
                            std::size_t k) {
    int best_cluster = 0;
    double best_distance = std::numeric_limits<double>::max();
    const double *sample = &data.values[sample_index * data.dim];

    for (std::size_t c = 0; c < k; ++c) {
        const double dist = squared_distance(sample, &centroids[c * data.dim], data.dim);
        if (dist < best_distance) {
            best_distance = dist;
            best_cluster = static_cast<int>(c);
        }
    }

    return best_cluster;
}

#endif
