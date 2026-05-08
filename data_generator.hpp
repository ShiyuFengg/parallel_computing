#ifndef DATA_GENERATOR_HPP
#define DATA_GENERATOR_HPP

#include <vector>
#include <random>
#include <cmath>

// Generate synthetic Gaussian cluster data
// N: total number of samples
// d: dimension
// K: number of clusters
// data: output flat array of size N*d
// labels: optional true labels of size N
inline void generate_data(int N, int d, int K,
                          std::vector<double>& data,
                          std::vector<int>* labels = nullptr,
                          unsigned int seed = 42) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> center_dist(-50.0, 50.0);
    std::normal_distribution<double> noise(0.0, 5.0);

    // Generate K cluster centers
    std::vector<double> centers(K * d);
    for (int k = 0; k < K; ++k) {
        for (int dim = 0; dim < d; ++dim) {
            centers[k * d + dim] = center_dist(rng);
        }
    }

    data.resize(N * d);
    if (labels) labels->resize(N);

    int per_cluster = N / K;
    int remainder = N % K;
    int idx = 0;
    for (int k = 0; k < K; ++k) {
        int count = per_cluster + (k < remainder ? 1 : 0);
        for (int i = 0; i < count; ++i) {
            for (int dim = 0; dim < d; ++dim) {
                data[idx * d + dim] = centers[k * d + dim] + noise(rng);
            }
            if (labels) (*labels)[idx] = k;
            ++idx;
        }
    }
}

#endif // DATA_GENERATOR_HPP
