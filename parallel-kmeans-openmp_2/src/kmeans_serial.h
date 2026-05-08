#ifndef KMEANS_SERIAL_H
#define KMEANS_SERIAL_H

#include "dataset.h"

#include <cstddef>
#include <cstdint>
#include <vector>

struct KMeansConfig {
    std::size_t k;
    int max_iter;
    double tol;
    int threads;
};

struct KMeansResult {
    int iterations = 0;
    double inertia = 0.0;
    std::uint64_t distance_calculations = 0;
    std::uint64_t skipped_points = 0;
    double pruning_rate = 0.0;
    std::vector<double> centroids;
    std::vector<int> labels;
};

KMeansResult kmeans_serial(const Dataset &data, const KMeansConfig &config);

#endif
