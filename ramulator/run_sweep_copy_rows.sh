#!/bin/bash

OUT_DIR="./out"
WORKLOAD="./workloads/401.bzip2"

if [ ! -f $WORKLOAD ]; then
    gzip -d $WORKLOAD.gz -c > $WORKLOAD
fi

mkdir -p $OUT_DIR


COPY_ROWS_PER_SA=1
while [ $COPY_ROWS_PER_SA -le 128 ]; do
    
    echo "Running $WORKLOAD with $COPY_ROWS_PER_SA copy rows per SA..."

    ./ramulator ./configs/CROW_configs/LPDDR4.cfg --mode=cpu \
        -t $WORKLOAD -p warmup_insts=50000000 -p expected_limit_insts=100000000 \
        -p weak_rows_per_SA=0 -p copy_rows_per_SA=$COPY_ROWS_PER_SA \
        --stats $OUT_DIR/$(basename ${WORKLOAD})-${COPY_ROWS_PER_SA}-copyrows.out

    let COPY_ROWS_PER_SA=COPY_ROWS_PER_SA*2
done
