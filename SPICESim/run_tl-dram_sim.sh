#!/bin/bash


SPICE_BIN="python ./LTspice-cli/run.py"
SPARSER_BIN="./parser/sparser"
GEN_CONFIG="./gen_config.sh"

CONFIG_FILE="./ltspice_config"

# CONFIGS
VDD="1.2"
PV_MOD="0.0"
INIT_CELL_CHARGE=0.8 # initial cell voltage proportional to VDD (for Cell0)
INIT_OTHER_CELLS_CHARGE=0.8 # initial cell voltage proportional to VDD (for other cells)
PER_CELL_RESISTANCE=60 #40
PSENSE_ACT="4ns"
NSENSE_ACT="4ns"
TECH_NODE="7"
CELL_CAP="16.7fF"
WL0_ACT_TIME="20ns"
OTHER_WLS_ACT_TIME="20ns"
CELLS_PER_BL_SHORT="8"
CELLS_PER_BL_LONG="1016"

CONFIGS="
	core_voltage=${VDD}V
	cell0_charge_state=${INIT_CELL_CHARGE}
    other_cells_charge_state=${INIT_OTHER_CELLS_CHARGE}
    pv_modifier=${PV_MOD}
    SA_eq_W=141nm
    R_per_cell=${PER_CELL_RESISTANCE}
    psense_act=${PSENSE_ACT}
    nsense_act=${NSENSE_ACT}
    tech_node=${TECH_NODE}
    cell_cap=${CELL_CAP}
    wl0_act_time=${WL0_ACT_TIME}
    other_wls_act_time=${OTHER_WLS_ACT_TIME}
    cells_per_BL_short=${CELLS_PER_BL_SHORT}
    cells_per_BL_long=${CELLS_PER_BL_LONG}
    "

[[ ${INIT_CELL_CHARGE} > 0.49 ]] && EXP_VAL="h" || EXP_VAL="l"

pids=""
OUT_DIR="./sim_results/tran_revision/tl_dram/short${CELLS_PER_BL_SHORT}_long${CELLS_PER_BL_LONG}-${EXP_VAL}/"
mkdir -p ${OUT_DIR}


DRAM_MODEL_SUBDIR=""
DRAM_MODEL_DIR="./DRAM_models/${DRAM_MODEL_SUBDIR}"

TMP_MODEL_DIR=./tmp_models_22nm_${VDD}V/${DRAM_MODEL_SUBDIR}
mkdir -p ${TMP_MODEL_DIR}

DRAM_MODEL=tl-dram_22nm

# Run short segment sim
MODEL_SUFFIX=_short
CONFIGS_T=$CONFIGS" wl0_act=1.0 wl1_act=0.0 isol_tran_act=400ns"
CF=${CONFIG_FILE}${MODEL_SUFFIX}
EXTRA_PARAMS="-f ${CF}"
${GEN_CONFIG} ${CF} ${CONFIGS_T}

DM=${DRAM_MODEL}${MODEL_SUFFIX}

cp ${DRAM_MODEL_DIR}/${DRAM_MODEL}.asc ${TMP_MODEL_DIR}/${DM}.asc && \
${SPICE_BIN} -m ${TMP_MODEL_DIR}/${DM}.asc -o ${OUT_DIR} ${EXTRA_PARAMS} &> /dev/null && \
${SPARSER_BIN} ${OUT_DIR}/${DM}.raw ${EXP_VAL} ${VDD} 0.93f > ${OUT_DIR}/${DM}.out &
pids="$pids $!"

# Run long segment sim
MODEL_SUFFIX=_long
CONFIGS_T=$CONFIGS" wl0_act=0.0 wl1_act=1.0 isol_tran_act=0ns"
CF=${CONFIG_FILE}${MODEL_SUFFIX}
EXTRA_PARAMS="-f ${CF}"
${GEN_CONFIG} ${CF} ${CONFIGS_T}

DM=${DRAM_MODEL}${MODEL_SUFFIX}

cp ${DRAM_MODEL_DIR}/${DRAM_MODEL}.asc ${TMP_MODEL_DIR}/${DM}.asc && \
${SPICE_BIN} -m ${TMP_MODEL_DIR}/${DM}.asc -o ${OUT_DIR} ${EXTRA_PARAMS} &> /dev/null && \
${SPARSER_BIN} ${OUT_DIR}/${DM}.raw ${EXP_VAL} ${VDD} 0.93f mb1 > ${OUT_DIR}/${DM}.out &
pids="$pids $!"

# Copy from long to short segment
MODEL_SUFFIX=_copy
CONFIGS_T=$CONFIGS" wl0_act=1.0 wl1_act=1.0 isol_tran_act=0ns wl0_act_time=38ns cell0_charge_state=0"
CF=${CONFIG_FILE}${MODEL_SUFFIX}
EXTRA_PARAMS="-f ${CF}"
${GEN_CONFIG} ${CF} ${CONFIGS_T}

DM=${DRAM_MODEL}${MODEL_SUFFIX}

cp ${DRAM_MODEL_DIR}/${DRAM_MODEL}.asc ${TMP_MODEL_DIR}/${DM}.asc && \
${SPICE_BIN} -m ${TMP_MODEL_DIR}/${DM}.asc -o ${OUT_DIR} ${EXTRA_PARAMS} &> /dev/null && \
${SPARSER_BIN} ${OUT_DIR}/${DM}.raw ${EXP_VAL} ${VDD} 0.93f mb1 > ${OUT_DIR}/${DM}.out &
pids="$pids $!"


wait $pids

rm -r -f ${TMP_MODEL_DIR}
