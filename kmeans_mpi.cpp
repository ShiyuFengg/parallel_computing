#include <mpi.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <random>
#include <algorithm>
#include "data_generator.hpp"

// MPI Parallel K-Means using PCAM methodology
// Usage: mpirun -np <P> ./kmeans_mpi <N> <d> <K> <max_iter> [seed]
// Timer excludes data generation, MPI_Init, argument parsing, and MPI_Finalize.
// Timer includes MPI_Scatterv, MPI_Bcast, local computation, MPI_Allreduce, and center updates.

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

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

    std::vector<double> data;
    std::vector<double> centers(K * d);

    // Root process generates data and randomly samples initial centers
    // (excluded from timing)
    if (rank == 0) {
        generate_data(N, d, K, data, nullptr, seed);

        std::mt19937 rng(seed);
        std::vector<int> indices(N);
        for (int i = 0; i < N; ++i) indices[i] = i;
        std::shuffle(indices.begin(), indices.end(), rng);

        for (int k = 0; k < K; ++k) {
            int idx = indices[k];
            for (int dim = 0; dim < d; ++dim) {
                centers[k * d + dim] = data[idx * d + dim];
            }
        }
    }

    // Partitioning: divide N samples among P processes
    int base = N / size;
    int rem = N % size;
    std::vector<int> sendcounts(size), displs(size);
    for (int i = 0; i < size; ++i) {
        sendcounts[i] = (base + (i < rem ? 1 : 0)) * d;
        displs[i] = (i == 0) ? 0 : displs[i - 1] + sendcounts[i - 1];
    }
    int local_N = sendcounts[rank] / d;

    std::vector<double> local_data(local_N * d);

    // Timing starts before Scatterv/Bcast (includes communication setup)
    MPI_Barrier(MPI_COMM_WORLD);
    auto t_start = std::chrono::high_resolution_clock::now();

    // Scatter data and broadcast centers (included in timing)
    MPI_Scatterv(data.empty() ? nullptr : data.data(),
                 sendcounts.data(), displs.data(), MPI_DOUBLE,
                 local_data.data(), local_N * d, MPI_DOUBLE,
                 0, MPI_COMM_WORLD);
    MPI_Bcast(centers.data(), K * d, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    std::vector<int> local_labels(local_N);
    std::vector<double> local_sum(K * d, 0.0);
    std::vector<int> local_count(K, 0);
    std::vector<double> global_sum(K * d, 0.0);
    std::vector<int> global_count(K, 0);

    for (int iter = 0; iter < max_iter; ++iter) {
        // Local assignment step
        for (int i = 0; i < local_N; ++i) {
            double min_dist = 1e300;
            int best = 0;
            for (int k = 0; k < K; ++k) {
                double dist = 0.0;
                for (int dim = 0; dim < d; ++dim) {
                    double diff = local_data[i * d + dim] - centers[k * d + dim];
                    dist += diff * diff;
                }
                if (dist < min_dist) {
                    min_dist = dist;
                    best = k;
                }
            }
            local_labels[i] = best;
        }

        // Reset local accumulators
        std::fill(local_sum.begin(), local_sum.end(), 0.0);
        std::fill(local_count.begin(), local_count.end(), 0);

        // Local update accumulation
        for (int i = 0; i < local_N; ++i) {
            int k = local_labels[i];
            for (int dim = 0; dim < d; ++dim) {
                local_sum[k * d + dim] += local_data[i * d + dim];
            }
            local_count[k]++;
        }

        // Communication: global reduction of sums and counts
        MPI_Allreduce(local_sum.data(), global_sum.data(), K * d,
                      MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        MPI_Allreduce(local_count.data(), global_count.data(), K,
                      MPI_INT, MPI_SUM, MPI_COMM_WORLD);

        // Update centers globally
        for (int k = 0; k < K; ++k) {
            if (global_count[k] > 0) {
                for (int dim = 0; dim < d; ++dim) {
                    centers[k * d + dim] = global_sum[k * d + dim] / global_count[k];
                }
            }
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    auto t_end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

    if (rank == 0) {
        std::cout << "[MPI] P=" << size << " N=" << N << " d=" << d
                  << " K=" << K << " iter=" << max_iter
                  << " time_ms=" << elapsed_ms << std::endl;
    }

    MPI_Finalize();
    return 0;
}
