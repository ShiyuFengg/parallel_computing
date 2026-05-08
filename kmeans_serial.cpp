#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <chrono>
#include <random>
#include <algorithm>
#include "data_generator.hpp"

// Serial K-Means
// Usage: ./kmeans_serial <N> <d> <K> <max_iter> [seed]
// Timer excludes data generation, includes center initialization and iterations.

int main(int argc, char* argv[]) {
    int N = 100000;
    int d = 16;
    int K = 8;
    int max_iter = 20;
    unsigned int seed = 42;

    if (argc >= 2) N = std::atoi(argv[1]);
    if (argc >= 3) d = std::atoi(argv[2]);
    if (argc >= 4) K = std::atoi(argv[3]);
    if (argc >= 5) max_iter = std::atoi(argv[4]);
    if (argc >= 6) seed = static_cast<unsigned int>(std::atoi(argv[5]));

    // Data generation (excluded from timing)
    std::vector<double> data;
    generate_data(N, d, K, data, nullptr, seed);

    // Random center initialization (included in timing)
    std::mt19937 rng(seed);
    std::vector<int> indices(N);
    for (int i = 0; i < N; ++i) indices[i] = i;
    std::shuffle(indices.begin(), indices.end(), rng);

    std::vector<double> centers(K * d);
    for (int k = 0; k < K; ++k) {
        int idx = indices[k];
        for (int dim = 0; dim < d; ++dim) {
            centers[k * d + dim] = data[idx * d + dim];
        }
    }

    std::vector<int> labels(N);
    std::vector<double> sum(K * d, 0.0);
    std::vector<int> count(K, 0);

    auto t_start = std::chrono::high_resolution_clock::now();

    for (int iter = 0; iter < max_iter; ++iter) {
        // Assignment step
        for (int i = 0; i < N; ++i) {
            double min_dist = 1e300;
            int best = 0;
            for (int k = 0; k < K; ++k) {
                double dist = 0.0;
                for (int dim = 0; dim < d; ++dim) {
                    double diff = data[i * d + dim] - centers[k * d + dim];
                    dist += diff * diff;
                }
                if (dist < min_dist) {
                    min_dist = dist;
                    best = k;
                }
            }
            labels[i] = best;
        }

        // Reset sum and count
        std::fill(sum.begin(), sum.end(), 0.0);
        std::fill(count.begin(), count.end(), 0);

        // Update step: accumulate
        for (int i = 0; i < N; ++i) {
            int k = labels[i];
            for (int dim = 0; dim < d; ++dim) {
                sum[k * d + dim] += data[i * d + dim];
            }
            count[k]++;
        }

        // Update centers
        for (int k = 0; k < K; ++k) {
            if (count[k] > 0) {
                for (int dim = 0; dim < d; ++dim) {
                    centers[k * d + dim] = sum[k * d + dim] / count[k];
                }
            }
        }
    }

    auto t_end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

    std::cout << "[Serial] N=" << N << " d=" << d << " K=" << K
              << " iter=" << max_iter
              << " time_ms=" << elapsed_ms << std::endl;

    return 0;
}
