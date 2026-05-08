# results

实验脚本 `scripts/run_experiments.sh` 会把结果保存到:

```text
results/result.csv
```

CSV 字段为:

```text
n,dim,k,mode,threads,time,iterations,inertia,distance_calculations,skipped_points,pruning_rate,speedup,efficiency
```

其中:

```text
speedup = T_serial / T_mode_threads
efficiency = speedup / threads
```

`mode` 为 `serial`、`parallel` 或 `optimized`。`serial` 和 `parallel` 的剪枝统计字段为 0，`optimized` 会输出 Hamerly 剪枝相关统计。
