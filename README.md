# 并行 K-Means 聚类算法设计与实现

基于 MPI 的并行 K-Means 聚类算法课程设计，使用 PCAM 方法学进行并行设计，并在本地 MacBook 上完成实验。

## 项目结构

```
parallel-kmeans/
├── README.md                   # 本文件
├── data_generator.hpp          # 合成高斯分布数据生成器
├── kmeans_serial.cpp           # 串行基线版本
├── kmeans_mpi.cpp              # MPI 并行版本
├── run_experiments.sh          # 批量实验脚本
├── plot_results.py             # 自动生成性能图表
├── generate_report.py          # 自动生成 Word 报告
├── results/                    # 实验结果
│   ├── experiment_results.csv  # 原始实验数据
│   ├── speedup.png             # 加速比曲线
│   ├── efficiency.png          # 效率曲线
│   ├── time.png                # 执行时间曲线
│   └── cost.png                # 成本曲线
├── report.md                   # Markdown 课程设计报告
└── 并行KMeans聚类课程设计报告.docx   # Word 报告
```

## 环境要求

- macOS（或 Linux）
- C++17 编译器（Apple Clang / GCC）
- OpenMPI
- `bc` 命令行计算器（实验脚本用）
- Python 3 + matplotlib（图表生成用）
- python-docx（Word 报告生成用）

## 安装 OpenMPI

```bash
brew install open-mpi
```

## 编译

```bash
mpicxx -O2 -std=c++17 -o kmeans_serial kmeans_serial.cpp
mpicxx -O2 -std=c++17 -o kmeans_mpi kmeans_mpi.cpp
```

## 运行

### 串行版本
```bash
./kmeans_serial <N> <d> <K> <max_iter> [seed]
# 示例
./kmeans_serial 100000 16 8 20 42
```

### MPI 并行版本
```bash
mpirun --oversubscribe -np <P> ./kmeans_mpi <N> <d> <K> <max_iter> [seed]
# 示例：4 进程
mpirun --oversubscribe -np 4 ./kmeans_mpi 100000 16 8 20 42
```

> `--oversubscribe` 允许在 MacBook 上启动超过物理核心数的进程，便于测试扩展性。

## 批量实验

```bash
bash run_experiments.sh
```

脚本会自动编译、运行不同数据规模（N）和进程数（P）的组合，每组配置**重复运行 5 次**取平均，计算加速比、效率和成本，结果保存在 `results/experiment_results.csv` 中。

## 生成图表

```bash
python3 plot_results.py
```

生成 4 张性能图表到 `results/` 目录。

## 生成 Word 报告

```bash
python3 generate_report.py
```

生成 `并行KMeans聚类课程设计报告.docx`。

## PCAM 设计概要

| 阶段 | 设计内容 |
|------|----------|
| **划分 (Partitioning)** | 域分解：将 N 个样本均匀划分为 P 个子集，每个子集分配到一个进程 |
| **通讯 (Communication)** | 每轮迭代使用 `MPI_Allreduce` 全局归约局部簇内和与计数，更新全局聚类中心 |
| **组合 (Agglomeration)** | 将单个样本的计算组合为进程级粗粒度任务，提升计算/通讯比 |
| **映射 (Mapping)** | 静态映射：进程 rank i 处理连续的样本块，负载均衡 |

## 实验设计说明

### 计时口径

- **串行版**：不计入数据生成时间，但计入初始中心设置和 K-Means 迭代计算时间。
- **MPI 版**：不计入数据生成、MPI_Init、参数解析、打印和 MPI_Finalize，但计入 MPI_Scatterv、MPI_Bcast、每轮本地计算、MPI_Allreduce 和中心更新时间。
- 加速比使用同一口径下的串行时间 Ts 和并行时间 Tp 计算。

### 初始中心选择

从全部样本中随机无重复采样 K 个样本作为初始聚类中心，使用固定随机种子（默认 42），保证串行版和 MPI 版初始化一致、实验结果可复现。

### 实验规模

| 参数 | 取值 |
|------|------|
| 样本数 N | 100000, 500000, 1000000, 2000000 |
| 维度 d | 16 |
| 聚类数 K | 8 |
| 最大迭代次数 | 20 |
| 进程数 P | 1, 2, 4, 8 |
| 重复次数 | 5 次（取平均） |

## 评估指标

| 指标 | 说明 |
|------|------|
| 运行时间 T | 算法执行时间（毫秒），5 次运行取平均 |
| 加速比 Sp | Sp = Ts / Tp |
| 效率 E | E = Sp / P |
| 成本 C | C = Tp × P |
