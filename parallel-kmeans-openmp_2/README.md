# parallel-kmeans-openmp

## 1. 项目简介

本项目是一个 C++17 + OpenMP 的并行 K-means 聚类课程项目。项目保留三种可对比实现:

| mode | 说明 |
| --- | --- |
| `serial` | 串行 K-means 基准版本 |
| `parallel` | 朴素 OpenMP 并行 K-means |
| `optimized` | Hamerly 剪枝 + SoA 数据布局 + padding 消除 false sharing 的优化版本 |

程序不依赖第三方库，只使用 C++ 标准库和 OpenMP。实验脚本会生成 `results/result.csv`，可直接用于课程报告中的运行时间、加速比、并行效率和剪枝率分析。

## 2. K-means 原理

K-means 是一种无监督聚类算法，目标是将 `N` 个样本划分为 `K` 个簇，使样本到所属簇中心的平方距离之和最小。该目标函数通常称为 SSE 或 inertia:

```text
SSE = sum_i ||x_i - c_label(i)||^2
```

基本步骤:

1. 初始化 `K` 个聚类中心。
2. 对每个样本计算其到所有中心的距离，并分配给最近中心。
3. 对每个簇计算样本均值，更新聚类中心。
4. 若中心移动距离小于 `tol`，或达到 `max_iter`，则停止迭代。

本项目中三种模式都使用相同随机种子生成数据，并使用前 `K` 个样本作为初始中心，保证实验对比公平。

## 3. 朴素 OpenMP 并行设计

朴素并行版本主要并行化样本分配阶段。每个样本寻找最近中心的计算互不依赖，因此可以使用 OpenMP 将样本循环划分给多个线程。

设计要点:

1. 使用 `omp_set_num_threads(threads)` 设置线程数。
2. 使用 `#pragma omp parallel` 和 `#pragma omp for schedule(static)` 并行遍历样本。
3. 每个线程维护独立的 `local_sum` 和 `local_count`。
4. 主样本循环中不使用 `atomic` 或 `critical` 更新全局统计。
5. 每轮结束后统一 reduce 局部统计数组，再更新全局聚类中心。

## 4. optimized 版本的三个创新点

### 4.1 Hamerly 三角不等式剪枝

optimized 版本为每个样本维护:

```text
label[i]  当前所属簇
upper[i]  样本到当前所属中心距离的上界
lower[i]  样本到其它中心距离的下界
```

每轮迭代先计算中心间距离，并得到:

```text
s[c] = 0.5 * min_{j != c} dist(center[c], center[j])
```

对样本 `i`，若满足:

```text
upper[i] <= max(lower[i], s[label[i]])
```

则根据三角不等式可判断该样本仍属于原中心，跳过完整的样本到所有中心距离计算。否则重新计算该样本到所有中心的距离，并更新最近中心、最近距离、第二近距离、`label`、`upper` 和 `lower`。

中心更新后，计算每个中心移动距离 `move[c]` 和最大移动距离 `max_move`，并更新上下界:

```text
upper[i] += move[label[i]]
lower[i] = max(0, lower[i] - max_move)
```

optimized 版本额外输出:

```text
distance_calculations  实际执行的样本-中心距离计算次数
skipped_points         被 Hamerly 条件剪枝跳过的样本访问次数
pruning_rate           skipped_points / 总样本访问次数
```

### 4.2 SoA 数据布局

原始朴素版本使用 AoS 布局:

```text
data_aos[i * dim + d]
```

即一个样本的所有维度连续存储。这种布局适合逐样本访问，但在优化版本中，为了让按维度和中心的距离累加更容易获得连续内存访问，额外保存 SoA 布局:

```text
data_soa[d * n + i]
```

SoA 表示同一维度的所有样本连续存储。optimized 版本的距离计算函数基于 SoA 实现，同时中心也按维度优先存储，使内层中心循环访问连续内存，有利于缓存命中和编译器 SIMD 自动向量化。

### 4.3 padding 消除 false sharing

false sharing 指多个线程写入不同变量，但这些变量位于同一个 cache line 中，导致缓存行在多个核心之间频繁失效和同步，降低并行性能。

optimized 版本中，每个线程使用独立的 `local_sum` 和 `local_count`，并使用 64 字节对齐和 padding:

```cpp
struct alignas(64) ThreadLocalStats { ... };
```

同时每个线程的局部数组按 cache line 对齐分配。这样主样本循环中每个线程只写自己的局部统计数组，最后统一 reduce，避免了 `atomic`、`critical` 和 false sharing 带来的额外开销。

## 5. 复杂度分析

设样本数量为 `N`，特征维度为 `D`，聚类数量为 `K`，迭代次数为 `I`，线程数为 `p`。

串行 K-means:

```text
O(I × N × K × D)
```

朴素 OpenMP 并行 K-means:

```text
O(I × (N × K × D / p + p × K × D))
```

其中 `N × K × D / p` 是并行样本分配阶段的理想计算量，`p × K × D` 是每轮合并线程局部统计数组的开销。

Hamerly 优化版本最坏情况下仍然需要对所有样本计算所有中心距离:

```text
O(I × N × K × D)
```

但实际运行中，大量样本会被三角不等式剪枝。设 `N_active` 为未被剪枝、仍需完整距离计算的样本数，则实际计算量近似为:

```text
O(I × N_active × K × D)
```

当聚类中心逐渐稳定时，`N_active` 通常会明显小于 `N`，因此优化版本可能减少大量距离计算。

空间复杂度方面，串行和朴素并行都需要存储数据、标签、中心和统计数组。optimized 版本额外维护 SoA 数据、`upper/lower` 边界、中心间距离辅助信息以及带 padding 的线程局部统计数组。

## 6. 运行时间、加速比和并行效率

设某个模式在 `p` 个线程下的运行时间为 `T_mode_threads`，同一数据规模下串行版本运行时间为 `T_serial`。

加速比:

```text
speedup = T_serial / T_mode_threads
```

并行效率:

```text
efficiency = speedup / threads
```

剪枝率:

```text
pruning_rate = skipped_points / 总样本访问次数
```

## 7. 编译与运行

编译:

```bash
make
```

Makefile 使用:

```text
-std=c++17 -O3 -march=native -fopenmp
```

运行串行版本:

```bash
./parallel_kmeans --mode serial --n 100000 --dim 10 --k 5 --max_iter 100 --tol 1e-4 --threads 1 --seed 42
```

运行朴素并行版本:

```bash
./parallel_kmeans --mode parallel --n 100000 --dim 10 --k 5 --max_iter 100 --tol 1e-4 --threads 4 --seed 42
```

运行 optimized 版本:

```bash
./parallel_kmeans --mode optimized --n 100000 --dim 10 --k 5 --max_iter 100 --tol 1e-4 --threads 4 --seed 42
```

程序统一输出 CSV 风格结果:

```text
mode,n,dim,k,threads,time,iterations,inertia,distance_calculations,skipped_points,pruning_rate
```

`serial` 和 `parallel` 没有剪枝，后三个剪枝统计字段输出 0。

## 8. 实验运行方法

运行自动实验脚本:

```bash
bash scripts/run_experiments.sh
```

脚本参数:

```text
n = 10000, 50000, 100000, 500000
dim = 10
k = 5
max_iter = 100
tol = 1e-4
threads = 1, 2, 4, 8
```

脚本对每个 `n` 先运行 `serial` 得到 `T_serial`，再运行 `parallel` 和 `optimized`，结果保存到:

```text
results/result.csv
```

CSV 表头:

```text
n,dim,k,mode,threads,time,iterations,inertia,distance_calculations,skipped_points,pruning_rate,speedup,efficiency
```

## 9. 实验结果

以下结果来自 `results/large_scale_result.txt`、`results/larger_scale_result.txt`、`results/experiment_500k_dim20_k20_t8.txt` 和 `results/experiment_500k_dim20_k20_t8_repeat.txt`。加速比以同一组实验中的 `serial` 时间为基准计算:

```text
speedup = T_serial / T_mode_threads
efficiency = speedup / threads
```

### 9.1 大规模样本实验

实验参数: `max_iter=100`，`tol=1e-4`。

| n | dim | k | mode | threads | time(s) | iterations | inertia | pruning_rate | speedup | efficiency |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 500000 | 10 | 5 | serial | 1 | 1.519776 | 100 | 2333468186.129767 | 0.000000 | 1.000 | 1.000 |
| 500000 | 10 | 5 | parallel | 4 | 0.591554 | 100 | 2333468186.129786 | 0.000000 | 2.569 | 0.642 |
| 500000 | 10 | 5 | optimized | 4 | 0.213770 | 100 | 2333468186.129786 | 0.979722 | 7.109 | 1.777 |
| 2000000 | 10 | 5 | serial | 1 | 6.112560 | 100 | 9330336541.025034 | 0.000000 | 1.000 | 1.000 |
| 2000000 | 10 | 5 | parallel | 4 | 2.515455 | 100 | 9330336541.025276 | 0.000000 | 2.430 | 0.608 |
| 2000000 | 10 | 5 | optimized | 4 | 0.867378 | 100 | 9330336541.025276 | 0.984227 | 7.047 | 1.762 |
| 2000000 | 10 | 5 | parallel | 8 | 1.877953 | 100 | 9330336541.025126 | 0.000000 | 3.255 | 0.407 |
| 2000000 | 10 | 5 | optimized | 8 | 0.813323 | 100 | 9330336541.025127 | 0.984227 | 7.516 | 0.939 |

对应的 Hamerly 剪枝统计如下:

| n | dim | k | threads | distance_calculations | skipped_points | pruning_rate |
| --- | --- | --- | --- | --- | --- | --- |
| 500000 | 10 | 5 | 4 | 10069615 | 48986077 | 0.979722 |
| 2000000 | 10 | 5 | 4 | 35773295 | 196845341 | 0.984227 |
| 2000000 | 10 | 5 | 8 | 35773295 | 196845341 | 0.984227 |

### 9.2 更高维度和更多聚类中心实验

实验参数: `n=500000`，`dim=20`，`k=20`，`threads=8`，`max_iter=100`，`tol=1e-4`，`seed=42`。

| run | mode | time(s) | iterations | inertia | distance_calculations | skipped_points | pruning_rate | speedup | efficiency |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | serial | 7.729622 | 100 | 6772740963.244875 | 0 | 0 | 0.000000 | 1.000 | 1.000 |
| 1 | parallel | 1.354467 | 100 | 6772740963.244867 | 0 | 0 | 0.000000 | 5.707 | 0.713 |
| 1 | optimized | 0.485556 | 100 | 6772740963.244866 | 55675900 | 48216205 | 0.964324 | 15.919 | 1.990 |
| 2 | serial | 6.611643 | 100 | 6772740963.244875 | 0 | 0 | 0.000000 | 1.000 | 1.000 |
| 2 | parallel | 0.902823 | 100 | 6772740963.244866 | 0 | 0 | 0.000000 | 7.323 | 0.915 |
| 2 | optimized | 0.382936 | 100 | 6772740963.244867 | 55675900 | 48216205 | 0.964324 | 17.266 | 2.158 |

### 9.3 结果小结

在 `n=500000, dim=10, k=5, threads=4` 的实验中，朴素并行版本相对串行获得 `2.569x` 加速，optimized 版本运行时间进一步降到 `0.213770s`，相对串行达到 `7.109x` 加速。optimized 的剪枝率为 `97.9722%`，说明 Hamerly 三角不等式剪枝跳过了大量样本访问，从而显著减少了完整距离计算。

在 `n=2000000, dim=10, k=5` 的更大规模实验中，optimized 版本在 4 线程和 8 线程下分别达到 `7.047x` 和 `7.516x` 加速，剪枝率均为 `98.4227%`。相比朴素并行，optimized 在 4 线程下从 `2.515455s` 降至 `0.867378s`，在 8 线程下从 `1.877953s` 降至 `0.813323s`。

在 `dim=20, k=20` 的高维、多中心实验中，optimized 两次重复实验分别达到 `15.919x` 和 `17.266x` 加速，剪枝率保持在 `96.4324%`。三种模式的 inertia 基本一致，说明优化主要改善运行时间和距离计算规模，没有改变聚类结果。

### 9.4 按课程定义计算加速比和效率

课程定义中，加速比和并行效率为:

```text
S_P = T1 / TP
E_P = S_P / P
```

其中 `T1` 是对应算法在单处理器上的运行时间，`TP` 是同一算法使用 `P` 个处理器时的运行时间。因此，对 `parallel` 模式，`T1` 取原始串行 K-means 的时间；对 `optimized` 模式，`T1` 取 optimized 单线程时间，这样可以把算法剪枝带来的收益和并行加速收益分开。

实验参数: `n=500000`，`dim=20`，`k=20`，`max_iter=100`，`tol=1e-4`，`seed=42`，`P=8`。

原始运行结果:

| mode | threads | time(s) | iterations | inertia | distance_calculations | skipped_points | pruning_rate |
| --- | --- | --- | --- | --- | --- | --- | --- |
| serial | 1 | 6.597250 | 100 | 6772740963.244875 | 0 | 0 | 0.000000 |
| parallel | 8 | 1.000968 | 100 | 6772740963.244866 | 0 | 0 | 0.000000 |
| optimized | 1 | 1.082308 | 100 | 6772740963.244874 | 55675900 | 48216205 | 0.964324 |
| optimized | 8 | 0.390763 | 100 | 6772740963.244866 | 55675900 | 48216205 | 0.964324 |

按课程定义计算:

| 对比对象 | T1(s) | TP(s) | P | 加速比 S_P | 并行效率 E_P |
| --- | --- | --- | --- | --- | --- |
| 朴素并行 parallel | 6.597250 | 1.000968 | 8 | 6.5909 | 0.8239 |
| 优化并行 optimized | 1.082308 | 0.390763 | 8 | 2.7697 | 0.3462 |

若从“原始串行算法到 optimized 8 线程”的整体性能提升角度看，还可以计算综合加速比:

```text
6.597250 / 0.390763 = 16.8830
```

但该值同时包含 Hamerly 剪枝、SoA 数据布局、padding 和 OpenMP 并行带来的综合收益，不应直接作为标准并行效率计算。

## 10. 结果分析写作建议

运行时间分析:

比较 `serial`、`parallel` 和 `optimized` 在不同 `n` 和线程数下的 `time`。通常数据规模越大，并行版本越容易摊薄线程调度和合并开销。

加速比分析:

对每个 `n` 使用同一规模下的 `T_serial` 作为基准，分析 `parallel` 和 `optimized` 的 `speedup`。若 optimized 加速比更高，可结合剪枝率和距离计算次数解释原因。

并行效率分析:

观察 `efficiency = speedup / threads`。线程数增加后效率可能下降，原因包括同步开销、内存带宽竞争、局部统计合并和不可并行部分。

剪枝效果分析:

重点分析 optimized 的 `distance_calculations`、`skipped_points` 和 `pruning_rate`。如果 `pruning_rate` 随迭代推进或数据规模增大而提高，说明 Hamerly 剪枝有效减少了完整距离计算。

SoA 与 padding 分析:

可以说明 SoA 主要改善内存访问连续性和 SIMD 自动向量化机会，padding 主要减少 false sharing。二者不改变算法结果，但会影响运行时间。

## 11. 总结

本项目在朴素并行 K-means 基础上新增 optimized 版本，综合使用 Hamerly 三角不等式剪枝、SoA 数据布局和 cache line padding。实验中可以从运行时间、加速比、并行效率、距离计算次数和剪枝率多个角度评价优化效果。
