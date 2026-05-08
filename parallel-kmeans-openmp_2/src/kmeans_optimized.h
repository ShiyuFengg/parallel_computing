#ifndef KMEANS_OPTIMIZED_H
#define KMEANS_OPTIMIZED_H

#include "kmeans_serial.h"

// Hamerly 剪枝 + SoA 数据布局 + cache line padding 的 OpenMP 优化版本。
KMeansResult kmeans_optimized(const Dataset &data, const KMeansConfig &config);

#endif
