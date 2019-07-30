#!/bin/bash

OUT_DIR="./out_CROW_TL-DRAM_SALP_comparison"
WORKLOAD="./workloads/401.bzip2"

if [ ! -f $WORKLOAD ]; then
    gzip -d $WORKLOAD.gz -c > $WORKLOAD
fi

mkdir -p $OUT_DIR


echo "(CROW-cache) Running $WORKLOAD with baseline configuration..."

./ramulator ./configs/CROW_configs/LPDDR4.cfg --mode=cpu \
    -t $WORKLOAD -p warmup_insts=50000000 -p expected_limit_insts=100000000 \
    -p weak_rows_per_SA=0 -p copy_rows_per_SA=0 \
    --stats $OUT_DIR/$(basename ${WORKLOAD})_baseline.out

    
COPY_ROWS_PER_SA=8
    
echo "(CROW-cache) Running $WORKLOAD with $COPY_ROWS_PER_SA copy rows per SA..."

./ramulator ./configs/CROW_configs/LPDDR4.cfg --mode=cpu \
    -t $WORKLOAD -p warmup_insts=50000000 -p expected_limit_insts=100000000 \
    -p weak_rows_per_SA=0 -p copy_rows_per_SA=$COPY_ROWS_PER_SA \
    --stats $OUT_DIR/$(basename ${WORKLOAD})_CROW-cache.out

echo "(TL-DRAM) Running $WORKLOAD with $COPY_ROWS_PER_SA rows in near segment..."

./ramulator ./configs/CROW_configs/LPDDR4_TL-DRAM.cfg --mode=cpu \
    -t $WORKLOAD -p warmup_insts=50000000 -p expected_limit_insts=100000000 \
    -p weak_rows_per_SA=0 -p copy_rows_per_SA=$COPY_ROWS_PER_SA \
    --stats $OUT_DIR/$(basename ${WORKLOAD})_TL-DRAM.out


echo "(SALP) Running $WORKLOAD with 8 subarrays per bank..."

./ramulator ./configs/CROW_configs/LPDDR4_SALP-MASA.cfg --mode=cpu \
    -t $WORKLOAD -p warmup_insts=50000000 -p expected_limit_insts=100000000 \
    --stats $OUT_DIR/$(basename ${WORKLOAD})_SALP.out

