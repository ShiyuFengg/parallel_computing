#!/bin/bash
# Experiment script for K-Means parallel algorithm
# Each configuration runs 5 times; reports mean and std
# N_values: 100000, 500000, 1000000, 2000000

set -e

cd "$(dirname "$0")"

# Compile both versions
echo "Compiling..."
mpicxx -O2 -std=c++17 -o kmeans_serial kmeans_serial.cpp
mpicxx -O2 -std=c++17 -o kmeans_mpi kmeans_mpi.cpp

# Experiment parameters
K=8
d=16
max_iter=20
seed=42
runs=5
N_values=(100000 500000 1000000 2000000)
P_values=(1 2 4 8)

RESULT_FILE="results/experiment_results.csv"
echo "N,d,K,max_iter,runs,seed,P,serial_time_mean,serial_time_std,mpi_time_mean,mpi_time_std,speedup,efficiency,cost" > "$RESULT_FILE"

for N in "${N_values[@]}"; do
    echo "========================================"
    echo "Running experiments for N=$N (runs=$runs)"
    echo "========================================"

    # --- Serial runs ---
    echo "  [Serial] N=$N ..."
    serial_times=()
    for r in $(seq 1 $runs); do
        serial_output=$(./kmeans_serial "$N" "$d" "$K" "$max_iter" "$seed")
        serial_time=$(echo "$serial_output" | grep -o 'time_ms=[0-9.]*' | cut -d= -f2)
        serial_times+=("$serial_time")
        echo "    Run $r: ${serial_time} ms"
    done

    # Compute serial mean and std
    serial_mean=$(echo "(${serial_times[*]})" | tr ' ' '+' | bc -l)
    serial_mean=$(echo "scale=4; $serial_mean / $runs" | bc)
    serial_sq_diff=0
    for t in "${serial_times[@]}"; do
        diff=$(echo "scale=8; $t - $serial_mean" | bc)
        sq=$(echo "scale=8; $diff * $diff" | bc)
        serial_sq_diff=$(echo "scale=8; $serial_sq_diff + $sq" | bc)
    done
    serial_std=$(echo "scale=4; sqrt($serial_sq_diff / $runs)" | bc)
    echo "    Serial mean: ${serial_mean} ms, std: ${serial_std} ms"

    # --- MPI runs ---
    for P in "${P_values[@]}"; do
        echo "  [MPI] P=$P N=$N ..."
        mpi_times=()
        for r in $(seq 1 $runs); do
            mpi_output=$(mpirun --oversubscribe -np "$P" ./kmeans_mpi "$N" "$d" "$K" "$max_iter" "$seed")
            mpi_time=$(echo "$mpi_output" | grep -o 'time_ms=[0-9.]*' | cut -d= -f2)
            mpi_times+=("$mpi_time")
            echo "    Run $r: ${mpi_time}ms"
        done

        # Compute MPI mean and std
        mpi_mean=$(echo "(${mpi_times[*]})" | tr ' ' '+' | bc -l)
        mpi_mean=$(echo "scale=4; $mpi_mean / $runs" | bc)
        mpi_sq_diff=0
        for t in "${mpi_times[@]}"; do
            diff=$(echo "scale=8; $t - $mpi_mean" | bc)
            sq=$(echo "scale=8; $diff * $diff" | bc)
            mpi_sq_diff=$(echo "scale=8; $mpi_sq_diff + $sq" | bc)
        done
        mpi_std=$(echo "scale=4; sqrt($mpi_sq_diff / $runs)" | bc)

        # Calculate metrics
        speedup=$(echo "scale=4; $serial_mean / $mpi_mean" | bc)
        efficiency=$(echo "scale=4; $speedup / $P" | bc)
        cost=$(echo "scale=4; $mpi_mean * $P" | bc)

        echo "    MPI mean: ${mpi_mean} ms, std: ${mpi_std} ms | Speedup: $speedup | Efficiency: $efficiency"
        echo "$N,$d,$K,$max_iter,$runs,$seed,$P,$serial_mean,$serial_std,$mpi_mean,$mpi_std,$speedup,$efficiency,$cost" >> "$RESULT_FILE"
    done
    echo ""
done

echo "========================================"
echo "All experiments completed."
echo "Results saved to: $RESULT_FILE"
echo "========================================"
