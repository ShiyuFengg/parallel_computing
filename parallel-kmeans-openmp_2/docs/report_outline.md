# 并行 K-means 优化算法课程报告提纲

## 1. 绪论

介绍聚类问题背景、K-means 的应用场景，以及大规模样本和高维特征下距离计算开销较大的问题。说明本项目在朴素 OpenMP 并行 K-means 基础上进一步设计 optimized 版本，用于提升性能并体现算法创新性。

## 2. K-means 算法原理

说明 SSE/inertia 目标函数、中心初始化、样本分配、中心更新和收敛条件。

可写公式:

```text
SSE = sum_i ||x_i - c_label(i)||^2
```

## 3. 串行 K-means 实现

说明串行版本的流程和作为实验基准的作用。强调所有模式使用相同随机种子和相同初始中心，保证公平比较。

## 4. 朴素 OpenMP 并行设计

说明样本分配阶段的并行性。描述每个线程使用独立 `local_sum` 和 `local_count`，主循环不使用 `atomic` 或 `critical`，最后统一 reduce。

可分析朴素并行复杂度:

```text
O(I × (N × K × D / p + p × K × D))
```

## 5. optimized 版本创新点一: Hamerly 三角不等式剪枝

说明每个样本维护:

```text
label[i], upper[i], lower[i]
```

每轮计算:

```text
s[c] = 0.5 * min_{j != c} dist(center[c], center[j])
```

剪枝判断:

```text
upper[i] <= max(lower[i], s[label[i]])
```

若满足条件则跳过完整距离计算，否则重新计算样本到所有中心的距离。中心更新后计算 `move[c]` 和 `max_move`，并更新上下界:

```text
upper[i] += move[label[i]]
lower[i] = max(0, lower[i] - max_move)
```

报告中可使用 `skipped_points`、`distance_calculations` 和 `pruning_rate` 分析剪枝效果。

## 6. optimized 版本创新点二: SoA 数据布局

对比 AoS 和 SoA:

```text
AoS: data_aos[i * dim + d]
SoA: data_soa[d * n + i]
```

说明 SoA 让同一维度的数据连续存储，optimized 距离计算基于 SoA，实现更连续的内存访问，并为 SIMD 自动向量化创造条件。

## 7. optimized 版本创新点三: padding 消除 false sharing

解释 false sharing: 多个线程写不同变量，但变量落在同一个 cache line 中，导致缓存行频繁失效。

说明本项目使用每线程独立局部统计数组，并使用 64 字节对齐和 padding。主样本循环中线程只写自己的局部数组，最后统一 reduce，避免 `atomic`、`critical` 和缓存行竞争。

## 8. 复杂度分析

串行:

```text
O(I × N × K × D)
```

朴素并行:

```text
O(I × (N × K × D / p + p × K × D))
```

Hamerly 优化最坏情况:

```text
O(I × N × K × D)
```

实际情况:

```text
O(I × N_active × K × D)
```

其中 `N_active` 为未被剪枝、仍需完整距离计算的样本数。

## 9. 实验环境

记录 CPU、内存、操作系统、g++ 版本、OpenMP 支持和编译参数:

```text
-std=c++17 -O3 -march=native -fopenmp
```

## 10. 实验参数

列出脚本参数:

| 参数 | 取值 |
| --- | --- |
| n | 10000, 50000, 100000, 500000 |
| dim | 10 |
| k | 5 |
| max_iter | 100 |
| tol | 1e-4 |
| threads | 1, 2, 4, 8 |
| mode | serial, parallel, optimized |

## 11. 运行时间对比

使用 `results/result.csv` 中的 `time` 字段绘制表格或折线图。比较三种模式在不同数据规模和线程数下的运行时间。

## 12. 加速比分析

公式:

```text
speedup = T_serial / T_mode_threads
```

对每个 `n` 分别以串行时间为基准，分析 `parallel` 和 `optimized` 的加速比。

## 13. 并行效率分析

公式:

```text
efficiency = speedup / threads
```

分析线程数增加时效率变化，解释同步、合并开销、内存带宽和 false sharing 优化的影响。

## 14. 剪枝率与距离计算分析

分析 optimized 输出的:

```text
distance_calculations
skipped_points
pruning_rate
```

说明剪枝率越高，完整距离计算越少，optimized 越可能相对朴素并行获得更好性能。

## 15. 总结与展望

总结三种实现的对比结论。展望可加入 K-means++ 初始化、真实数据集读取、更多 OpenMP 调度策略、自动绘图和更多硬件平台测试。
