# parallel-kmeans-openmp

## 项目简介

本项目是并行计算课程期末项目示例，使用 C++17 实现 K-means 聚类算法，并提供串行版本和基于 OpenMP 的并行版本。程序支持随机生成二维或多维数据集，可以通过命令行设置样本数量 `n`、特征维度 `dim`、聚类数量 `k`、最大迭代次数 `max_iter`、收敛阈值 `tol`、线程数 `threads` 和运行模式 `mode`。

项目不依赖第三方库，只使用 C++ 标准库和 OpenMP，适合在 Linux 或 Windows MinGW-w64/MSYS2 环境下使用 `g++` 编译运行。

## K-means 算法原理

K-means 是一种无监督聚类算法，目标是把 `n` 个样本划分为 `k` 个簇，使每个样本到所属簇中心的平方距离之和尽可能小。该目标函数通常称为 SSE 或 inertia:

```text
SSE = sum_i ||x_i - c_label(i)||^2
```

基本流程如下:

1. 初始化 `k` 个聚类中心。
2. 对每个样本计算其到所有聚类中心的距离，并分配给最近的中心。
3. 对每个簇计算簇内样本均值，并用均值更新聚类中心。
4. 若聚类中心最大移动距离小于 `tol`，或达到 `max_iter`，算法停止。

本项目中，串行版本和并行版本都使用同一份随机数据，并使用前 `k` 个样本作为初始聚类中心。只要 `--seed` 相同，两种模式的数据和初始中心就完全一致，便于公平比较运行时间。

## 并行化设计思路

K-means 每轮迭代中最耗时的部分通常是样本分配阶段。对每个样本寻找最近聚类中心的过程相互独立，因此适合使用 OpenMP 并行化。

并行版本的设计如下:

1. 使用 `omp_set_num_threads(threads)` 设置线程数。
2. 使用 `#pragma omp parallel` 和 `#pragma omp for schedule(static)` 并行遍历样本。
3. 每个线程维护自己的 `local_sums` 和 `local_counts`。
4. 样本分配时，线程只写自己的局部统计数组，避免数据竞争。
5. 并行分配结束后，合并所有线程的局部统计结果。
6. 根据全局 `sums` 和 `counts` 更新聚类中心，并判断是否收敛。

这种方法避免了在样本循环中频繁使用锁或临界区，适合样本数量较大的共享内存并行场景。

## OpenMP 使用说明

编译时需要添加 OpenMP 选项:

```bash
g++ -std=c++17 -O2 -fopenmp ...
```

本项目中的核心 OpenMP 代码形式如下:

```cpp
omp_set_num_threads(thread_count);

#pragma omp parallel
{
    #pragma omp for schedule(static)
    for (...) {
        // 并行分配样本，并写入线程局部 sum/count
    }
}
```

最终 SSE/inertia 的计算使用 OpenMP reduction:

```cpp
#pragma omp parallel for reduction(+:inertia)
```

## 项目结构

```text
parallel-kmeans-openmp/
├── README.md
├── Makefile
├── src/
│   ├── main.cpp
│   ├── kmeans_serial.cpp
│   ├── kmeans_serial.h
│   ├── kmeans_openmp.cpp
│   ├── kmeans_openmp.h
│   ├── dataset.cpp
│   ├── dataset.h
│   ├── timer.h
│   └── metrics.h
├── data/
│   └── README.md
├── results/
│   └── README.md
├── docs/
│   └── report_outline.md
└── scripts/
    └── run_experiments.sh
```

## 编译方法

Linux:

```bash
cd parallel-kmeans-openmp
make
```

Windows MinGW-w64/MSYS2:

```bash
cd parallel-kmeans-openmp
mingw32-make
```

清理编译产物:

```bash
make clean
```

## 运行方法

查看帮助:

```bash
./parallel_kmeans --help
```

运行串行版本:

```bash
./parallel_kmeans --mode serial --n 100000 --dim 10 --k 5 --max_iter 100 --tol 1e-4 --threads 1 --seed 42
```

运行 OpenMP 并行版本:

```bash
./parallel_kmeans --mode parallel --n 100000 --dim 10 --k 5 --max_iter 100 --tol 1e-4 --threads 4 --seed 42
```

Windows 下如果生成了 `.exe` 文件:

```bash
./parallel_kmeans.exe --mode parallel --n 100000 --dim 10 --k 5 --max_iter 100 --tol 1e-4 --threads 4 --seed 42
```

程序输出为 CSV 单行:

```text
mode,n,dim,k,max_iter,threads,iterations,time,inertia
parallel,100000,10,5,100,4,12,0.123456789,1234567.890000
```

字段含义:

| 字段 | 含义 |
| --- | --- |
| mode | 算法模式，`serial` 或 `parallel` |
| n | 样本数量 |
| dim | 特征维度 |
| k | 聚类数量 |
| max_iter | 最大迭代次数 |
| threads | 线程数 |
| iterations | 实际迭代次数 |
| time | 聚类算法运行时间，单位为秒 |
| inertia | 最终 SSE，即样本到所属聚类中心的平方距离之和 |

## 实验方法

运行自动实验脚本:

```bash
bash scripts/run_experiments.sh
```

脚本固定测试参数:

```text
n = 10000, 50000, 100000, 500000
p = 1, 2, 4, 8
dim = 10
k = 5
max_iter = 100
tol = 1e-4
seed = 42
```

结果保存到:

```text
results/result.csv
```

CSV 格式为:

```text
n,dim,k,mode,threads,time,iterations,inertia,speedup,efficiency
```

脚本对每一个数据规模 `n` 都先运行一次串行版本，得到同一规模下的基准时间 `T_serial`。随后运行 `p = 1, 2, 4, 8` 的并行版本，并计算加速比和并行效率。

## 加速比和并行效率

设某一数据规模下的串行运行时间为 `T_serial`，使用 `p` 个线程的并行运行时间为 `T_parallel(p)`。

加速比:

```text
Speedup(p) = T_serial / T_parallel(p)
```

并行效率:

```text
Efficiency(p) = Speedup(p) / p
```

如果 `Speedup(4) = 3.2`，则 `Efficiency(4) = 3.2 / 4 = 0.8`。效率越接近 1，说明线程利用越充分。

## 复杂度分析

设样本数量为 N，特征维度为 D，聚类数量为 K，迭代次数为 I。

### 串行算法时间复杂度

在每轮迭代中，K-means 需要计算每个样本到所有聚类中心的距离。每次距离计算需要遍历 D 个维度，因此每轮迭代的时间复杂度为：

```text
O(N × K × D)
```

若算法迭代 I 轮，则串行 K-means 的总时间复杂度为：

```text
O(I × N × K × D)
```

### 并行算法时间复杂度

在 OpenMP 并行版本中，样本分配阶段被划分给 p 个线程并行执行。理想情况下，每个线程处理 N / p 个样本，因此距离计算部分的时间复杂度约为：

```text
O(N × K × D / p)
```

同时，每个线程需要维护局部的聚类中心累加和与样本数量，最后需要将局部统计结果合并。合并阶段的复杂度约为：

```text
O(p × K × D)
```

因此，每轮并行 K-means 的时间复杂度可表示为：

```text
O(N × K × D / p + p × K × D)
```

总时间复杂度为：

```text
O(I × (N × K × D / p + p × K × D))
```

当 N 较大时，距离计算部分占主要开销，并行算法可以获得较好的加速效果；当 N 较小时，线程创建、同步和局部结果合并等开销占比较高，并行效率可能下降。

### 空间复杂度

串行算法需要存储样本数据、聚类标签和聚类中心，空间复杂度约为：

```text
O(N × D + K × D + N)
```

并行算法额外需要为每个线程维护局部统计数组 local_sum 和 local_count，因此额外空间复杂度为：

```text
O(p × K × D + p × K)
```

整体空间复杂度为：

```text
O(N × D + K × D + N + p × K × D)
```

## 实验结果分析

运行时间分析:

随着 `n` 从 10000 增加到 500000，串行和并行运行时间都会增长。由于 K-means 的主导计算量近似正比于 `n * k * dim`，在 `dim`、`k`、`max_iter` 固定时，运行时间通常随样本数量近似线性增加。

加速比分析:

加速比反映并行版本相对串行版本的性能提升。理论上，线程数增加会降低样本分配阶段耗时，但实际加速比通常小于线程数 `p`，因为中心更新、结果合并、同步和内存访问无法完全并行化。

并行效率分析:

并行效率为 `Speedup / p`，用于衡量每个线程的平均利用率。随着线程数增加，并行效率通常会下降，这是因为线程越多，同步、合并和内存带宽竞争带来的额外开销越明显。

数据规模对并行效果的影响:

数据规模较小时，单轮计算量有限，OpenMP 调度、同步和局部数组合并开销占比较高，可能出现并行版本提升不明显甚至慢于串行版本的情况。数据规模增大后，样本分配阶段的计算量占比提高，并行版本更容易获得明显加速。

线程数对并行效果的影响:

从 1 个线程增加到 2 个或 4 个线程时，通常可以观察到较明显的运行时间下降。继续增加到 8 个线程后，如果硬件核心数不足，或内存带宽成为瓶颈，加速比增长会放缓，并行效率会下降。

## 实验结果表格模板

| n | dim | k | mode | threads | time | iterations | inertia | speedup | efficiency |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 10000 | 10 | 5 | serial | 1 |  |  |  | 1.000000 | 1.000000 |
| 10000 | 10 | 5 | parallel | 1 |  |  |  |  |  |
| 10000 | 10 | 5 | parallel | 2 |  |  |  |  |  |
| 10000 | 10 | 5 | parallel | 4 |  |  |  |  |  |
| 10000 | 10 | 5 | parallel | 8 |  |  |  |  |  |

课程报告中可以按不同 `n` 分组，比较不同线程数下的运行时间、加速比和并行效率，并结合复杂度分析解释实验现象。

## 总结

本项目实现了完整的串行 K-means 和 OpenMP 并行 K-means。并行版本将样本分配阶段作为主要并行对象，并通过线程局部统计数组避免数据竞争。实验中通常可以观察到，样本规模越大，并行计算越容易摊薄线程同步和结果合并开销；线程数增加可以降低运行时间，但加速比和并行效率会受到不可并行部分、内存带宽和同步开销限制。

后续可以扩展真实数据集读取、更多初始化方法、不同 OpenMP 调度策略对比，以及使用图表自动生成实验报告。
