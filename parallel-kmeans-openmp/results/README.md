# results

实验脚本 `scripts/run_experiments.sh` 会把结果保存到:

```text
results/result.csv
```

CSV 字段为:

```text
n,dim,k,mode,threads,time,iterations,inertia,speedup,efficiency
```

其中 `speedup = T_serial / T_parallel(p)`，`efficiency = speedup / p`。
