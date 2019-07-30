#!/bin/bash


SPARSER_BIN="./parser/sparser"


# PARAMS
TARGET_TRAS="0.93"
VDD="1.2"
INIT_CELL_CHARGE="0.8"

[[ ${INIT_CELL_CHARGE} > 0.49 ]] && EXP_VAL="h" || EXP_VAL="l"

RES_DIR=$1
OUT_DIR=$2

DRAM_MODEL=crow_22nm_wl_model

while (( $(bc -l <<< "$TARGET_TRAS >= 0.67") )); do
	
	echo "Running target tRAS: $TARGET_TRAS"

    # Run single-cell sim
    MODEL_SUFFIX=_1c
    DM=${DRAM_MODEL}${MODEL_SUFFIX}

    ${SPARSER_BIN} ${RES_DIR}/${DM}.raw ${EXP_VAL} ${VDD} ${TARGET_TRAS} > ${OUT_DIR}/${DM}_$TARGET_TRAS.out &
    pids="$pids $!"
    
    # Run two-cell sim
    MODEL_SUFFIX=_2c
    DM=${DRAM_MODEL}${MODEL_SUFFIX}
    
    ${SPARSER_BIN} ${RES_DIR}/${DM}.raw ${EXP_VAL} ${VDD} ${TARGET_TRAS} > ${OUT_DIR}/${DM}_$TARGET_TRAS.out &
    pids="$pids $!"
    
    # Run three-cell sim
    MODEL_SUFFIX=_3c
    DM=${DRAM_MODEL}${MODEL_SUFFIX}
    
    ${SPARSER_BIN} ${RES_DIR}/${DM}.raw ${EXP_VAL} ${VDD} ${TARGET_TRAS} > ${OUT_DIR}/${DM}_$TARGET_TRAS.out &
    pids="$pids $!"
    
    # Run four-cell sim
    MODEL_SUFFIX=_4c
    DM=${DRAM_MODEL}${MODEL_SUFFIX}
    
    ${SPARSER_BIN} ${RES_DIR}/${DM}.raw ${EXP_VAL} ${VDD} ${TARGET_TRAS} > ${OUT_DIR}/${DM}_$TARGET_TRAS.out &
    pids="$pids $!"
    
    # Run five-cell sim
    MODEL_SUFFIX=_5c
    DM=${DRAM_MODEL}${MODEL_SUFFIX}
    
    ${SPARSER_BIN} ${RES_DIR}/${DM}.raw ${EXP_VAL} ${VDD} ${TARGET_TRAS} > ${OUT_DIR}/${DM}_$TARGET_TRAS.out &
    pids="$pids $!"
    
    # Run six-cell sim
    MODEL_SUFFIX=_6c
    DM=${DRAM_MODEL}${MODEL_SUFFIX}
    
    ${SPARSER_BIN} ${RES_DIR}/${DM}.raw ${EXP_VAL} ${VDD} ${TARGET_TRAS} > ${OUT_DIR}/${DM}_$TARGET_TRAS.out &
    pids="$pids $!"
    
    # Run seven-cell sim
    MODEL_SUFFIX=_7c
    DM=${DRAM_MODEL}${MODEL_SUFFIX}
    
    ${SPARSER_BIN} ${RES_DIR}/${DM}.raw ${EXP_VAL} ${VDD} ${TARGET_TRAS} > ${OUT_DIR}/${DM}_$TARGET_TRAS.out &
    pids="$pids $!"
    
    # Run eight-cell sim
    MODEL_SUFFIX=_8c
    DM=${DRAM_MODEL}${MODEL_SUFFIX}
    
    ${SPARSER_BIN} ${RES_DIR}/${DM}.raw ${EXP_VAL} ${VDD} ${TARGET_TRAS} > ${OUT_DIR}/${DM}_$TARGET_TRAS.out &
    pids="$pids $!"
    
    TARGET_TRAS=$(bc -l <<< "$TARGET_TRAS - 0.01")

done

wait $pids

