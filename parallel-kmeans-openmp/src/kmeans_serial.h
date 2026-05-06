#ifndef KMEANS_SERIAL_H
#define KMEANS_SERIAL_H

#include "dataset.h"

#include <cstddef>
#include <vector>

struct KMeansConfig {
    std::size_t k;
    int max_iter;
    double tol;
    int threads;
};

struct KMeansResult {
    int iterations;
    double inertia;
    std::vector<double> centroids;
    std::vector<int> labels;
};

KMeansResult kmeans_serial(const Dataset &data, const KMeansConfig &config);

#endif
