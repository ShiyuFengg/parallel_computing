#ifndef DATASET_H
#define DATASET_H

#include <cstddef>
#include <cstdint>
#include <vector>

struct Dataset {
    std::size_t n;
    std::size_t dim;
    std::vector<double> values; // 按行存储: 第 i 个样本第 d 维为 values[i * dim + d]
};

// 随机生成多维聚类数据。k 只用于生成隐藏的真实簇中心，使数据更适合 K-means 实验。
Dataset generate_random_dataset(std::size_t n, std::size_t dim, std::size_t k, std::uint32_t seed = 42);

#endif
