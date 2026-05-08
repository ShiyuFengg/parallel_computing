#!/usr/bin/env python3
"""Generate plots for K-Means parallel algorithm experiments."""

import csv
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from collections import defaultdict

data = defaultdict(lambda: defaultdict(dict))
with open('results/experiment_results.csv', newline='') as f:
    reader = csv.DictReader(f)
    for row in reader:
        N = int(row['N'])
        P = int(row['P'])
        data[N][P] = {
            'serial_mean': float(row['serial_time_mean']),
            'mpi_mean': float(row['mpi_time_mean']),
            'speedup': float(row['speedup']),
            'efficiency': float(row['efficiency']),
            'cost': float(row['cost']),
        }

N_values = sorted(data.keys())
P_values = sorted({P for nd in data.values() for P in nd.keys()})
colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728']

# Plot 1: Speedup vs P
fig, ax = plt.subplots(figsize=(8, 6))
for i, N in enumerate(N_values):
    speeds = [data[N][P]['speedup'] for P in P_values]
    ax.plot(P_values, speeds, marker='o', label=f'N={N}', color=colors[i % len(colors)], linewidth=2)
ax.plot(P_values, P_values, 'k--', label='Ideal (Sp=P)', linewidth=1.5, alpha=0.7)
ax.set_xlabel('Number of Processes (P)', fontsize=12)
ax.set_ylabel('Speedup (Sp)', fontsize=12)
ax.set_title('K-Means Parallel Speedup', fontsize=14)
ax.legend(loc='upper left')
ax.grid(True, alpha=0.3)
ax.set_xticks(P_values)
plt.tight_layout()
plt.savefig('results/speedup.png', dpi=150)
plt.close()
print("Saved: results/speedup.png")

# Plot 2: Efficiency vs P
fig, ax = plt.subplots(figsize=(8, 6))
for i, N in enumerate(N_values):
    effs = [data[N][P]['efficiency'] for P in P_values]
    ax.plot(P_values, effs, marker='s', label=f'N={N}', color=colors[i % len(colors)], linewidth=2)
ax.axhline(y=1.0, color='k', linestyle='--', label='Ideal (E=1)', linewidth=1.5, alpha=0.7)
ax.set_xlabel('Number of Processes (P)', fontsize=12)
ax.set_ylabel('Efficiency (E)', fontsize=12)
ax.set_title('K-Means Parallel Efficiency', fontsize=14)
ax.legend(loc='upper right')
ax.grid(True, alpha=0.3)
ax.set_xticks(P_values)
ax.set_ylim(0, 1.4)
plt.tight_layout()
plt.savefig('results/efficiency.png', dpi=150)
plt.close()
print("Saved: results/efficiency.png")

# Plot 3: Time vs P
fig, ax = plt.subplots(figsize=(8, 6))
for i, N in enumerate(N_values):
    times = [data[N][P]['mpi_mean'] for P in P_values]
    ax.plot(P_values, times, marker='^', label=f'N={N}', color=colors[i % len(colors)], linewidth=2)
ax.set_xlabel('Number of Processes (P)', fontsize=12)
ax.set_ylabel('Execution Time (ms)', fontsize=12)
ax.set_title('K-Means Parallel Execution Time', fontsize=14)
ax.legend(loc='upper right')
ax.grid(True, alpha=0.3)
ax.set_xticks(P_values)
plt.tight_layout()
plt.savefig('results/time.png', dpi=150)
plt.close()
print("Saved: results/time.png")

# Plot 4: Cost vs P
fig, ax = plt.subplots(figsize=(8, 6))
for i, N in enumerate(N_values):
    costs = [data[N][P]['cost'] for P in P_values]
    ax.plot(P_values, costs, marker='d', label=f'N={N}', color=colors[i % len(colors)], linewidth=2)
ax.set_xlabel('Number of Processes (P)', fontsize=12)
ax.set_ylabel('Cost (Tp × P)', fontsize=12)
ax.set_title('K-Means Parallel Cost', fontsize=14)
ax.legend(loc='upper left')
ax.grid(True, alpha=0.3)
ax.set_xticks(P_values)
plt.tight_layout()
plt.savefig('results/cost.png', dpi=150)
plt.close()
print("Saved: results/cost.png")

print("\nAll plots generated successfully.")
