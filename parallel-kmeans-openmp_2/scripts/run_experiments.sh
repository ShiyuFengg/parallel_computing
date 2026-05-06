#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_ROOT"

mkdir -p results
RESULT_FILE="results/result.csv"

# 可通过环境变量覆盖默认实验参数，例如: DIM=16 K=8 bash scripts/run_experiments.sh
DIM="${DIM:-10}"
K="${K:-5}"
MAX_ITER="${MAX_ITER:-100}"
TOL="${TOL:-1e-4}"
SEED="${SEED:-42}"

NS=(10000 50000 100000 500000)
THREADS=(1 2 4 8)

make

BIN="./parallel_kmeans"
if [[ -x "./parallel_kmeans.exe" ]]; then
    BIN="./parallel_kmeans.exe"
fi

echo "n,dim,k,mode,threads,time,iterations,inertia,distance_calculations,skipped_points,pruning_rate,speedup,efficiency" > "$RESULT_FILE"

calc_div() {
    awk -v a="$1" -v b="$2" 'BEGIN {
        if (b == 0) {
            printf "0.000000";
        } else {
            printf "%.6f", a / b;
        }
    }'
}

append_result() {
    local raw="$1"
    local speedup="$2"
    local efficiency="$3"

    # 程序输出格式:
    # mode,n,dim,k,threads,time,iterations,inertia,distance_calculations,skipped_points,pruning_rate
    IFS=',' read -r mode_out n_out dim_out k_out threads_out time_out iterations_out inertia_out distance_calculations_out skipped_points_out pruning_rate_out <<< "$raw"
    echo "${n_out},${dim_out},${k_out},${mode_out},${threads_out},${time_out},${iterations_out},${inertia_out},${distance_calculations_out},${skipped_points_out},${pruning_rate_out},${speedup},${efficiency}" >> "$RESULT_FILE"
}

for n in "${NS[@]}"; do
    echo "Running serial: n=${n}, dim=${DIM}, k=${K}"
    serial_raw="$("$BIN" --mode serial \
                          --n "$n" \
                          --dim "$DIM" \
                          --k "$K" \
                          --max_iter "$MAX_ITER" \
                          --tol "$TOL" \
                          --threads 1 \
                          --seed "$SEED")"
    IFS=',' read -r _mode _n _dim _k _threads serial_time _iterations _inertia _distance_calculations _skipped_points _pruning_rate <<< "$serial_raw"
    append_result "$serial_raw" "1.000000" "1.000000"

    for threads in "${THREADS[@]}"; do
        echo "Running parallel: n=${n}, dim=${DIM}, k=${K}, threads=${threads}"
        parallel_raw="$("$BIN" --mode parallel \
                                --n "$n" \
                                --dim "$DIM" \
                                --k "$K" \
                                --max_iter "$MAX_ITER" \
                                --tol "$TOL" \
                                --threads "$threads" \
                                --seed "$SEED")"
        IFS=',' read -r _mode _n _dim _k _threads parallel_time _iterations _inertia _distance_calculations _skipped_points _pruning_rate <<< "$parallel_raw"
        speedup="$(calc_div "$serial_time" "$parallel_time")"
        efficiency="$(calc_div "$speedup" "$threads")"
        append_result "$parallel_raw" "$speedup" "$efficiency"

        echo "Running optimized: n=${n}, dim=${DIM}, k=${K}, threads=${threads}"
        optimized_raw="$("$BIN" --mode optimized \
                                 --n "$n" \
                                 --dim "$DIM" \
                                 --k "$K" \
                                 --max_iter "$MAX_ITER" \
                                 --tol "$TOL" \
                                 --threads "$threads" \
                                 --seed "$SEED")"
        IFS=',' read -r _mode _n _dim _k _threads optimized_time _iterations _inertia _distance_calculations _skipped_points _pruning_rate <<< "$optimized_raw"
        speedup="$(calc_div "$serial_time" "$optimized_time")"
        efficiency="$(calc_div "$speedup" "$threads")"
        append_result "$optimized_raw" "$speedup" "$efficiency"
    done
done

echo "Results saved to $RESULT_FILE"
