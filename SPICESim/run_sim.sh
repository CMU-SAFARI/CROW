#!/bin/bash

export DISPLAY=:0 # this is needed to be able to run LTSPICE over ssh

SPICE_BIN="python ./LTspice-cli/run.py"
SPARSER_BIN="./parser/sparser"
GEN_CONFIG="./gen_config.sh"

CONFIG_FILE="./ltspice_config"

# CONFIGS
VDD="1.2"
PV_MOD="0.0"
INIT_CELL_CHARGE=0.8 # initial cell voltage proportional to VDD (for Cell0)
INIT_OTHER_CELLS_CHARGE=0.8 # initial cell voltage proportional to VDD (for other cells)
PER_CELL_RESISTANCE=60 
PSENSE_ACT="4ns"
NSENSE_ACT="4ns"
TECH_NODE="7"
CELL_CAP="16.7fF"
BITLINE_CAP="83.5fF" # 5 times of CELL_CAP
WL0_ACT_TIME="20ns"
OTHER_WLS_ACT_TIME="20ns" # Should be 20ns + 18ns (tRCD) when simulating the CROW-copy case
                          # Should be 20ns when simulating the CROW-hit case
CSEL_ACT="120ns"

[[ ${INIT_CELL_CHARGE} > 0.49 ]] && WR0_VOLT="$VDD" WR1_VOLT="0V" || \
                                    WR1_VOLT="$VDD" WR0_VOLT="0V"

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
    bitline_cap=${BITLINE_CAP}
    csel_act=${CSEL_ACT}
    wr0_voltage=${WR0_VOLT}
    wr1_voltage=${WR1_VOLT}
    wl0_act_time=${WL0_ACT_TIME}
    other_wls_act_time=${OTHER_WLS_ACT_TIME}
    "


[[ ${INIT_CELL_CHARGE} > 0.49 ]] && EXP_VAL="h" || EXP_VAL="l"

pids=""
OUT_DIR="./sim_results/${TECH_NODE}tech_${VDD}V_${INIT_CELL_CHARGE}${EXP_VAL}_${PSENSE_ACT}_gap${OTHER_WLS_ACT_TIME}/"
mkdir -p ${OUT_DIR}


DRAM_MODEL_SUBDIR=""
DRAM_MODEL_DIR="./DRAM_models/${DRAM_MODEL_SUBDIR}"

TMP_MODEL_DIR=./tmp_models_22nm_${VDD}V/${DRAM_MODEL_SUBDIR}
mkdir -p ${TMP_MODEL_DIR}

DRAM_MODEL=crow_22nm_wl_model

# Run single-cell sim
MODEL_SUFFIX=_1c
CONFIGS_T=$CONFIGS" wl0_act=1.0 wl1_act=0 wl2_act=0 wl3_act=0
                  wl4_act=0 wl5_act=0 wl6_act=0 wl7_act=0"
CF=${CONFIG_FILE}${MODEL_SUFFIX}
EXTRA_PARAMS="-f ${CF}"
${GEN_CONFIG} ${CF} ${CONFIGS_T}

DM=${DRAM_MODEL}${MODEL_SUFFIX}

cp ${DRAM_MODEL_DIR}/${DRAM_MODEL}.asc ${TMP_MODEL_DIR}/${DM}.asc && \
${SPICE_BIN} -m ${TMP_MODEL_DIR}/${DM}.asc -o ${OUT_DIR} ${EXTRA_PARAMS} &> /dev/null && \
${SPARSER_BIN} ${OUT_DIR}/${DM}.raw ${EXP_VAL} ${VDD} > ${OUT_DIR}/${DM}.out &
pids="$pids $!"

# Run two-cell sim
MODEL_SUFFIX=_2c
CONFIGS_T=$CONFIGS" wl0_act=1.0 wl1_act=1.0 wl2_act=0 wl3_act=0
                  wl4_act=0 wl5_act=0 wl6_act=0 wl7_act=0"
CF=${CONFIG_FILE}${MODEL_SUFFIX}
EXTRA_PARAMS="-f ${CF}"
${GEN_CONFIG} ${CF} ${CONFIGS_T}

DM=${DRAM_MODEL}${MODEL_SUFFIX}

cp ${DRAM_MODEL_DIR}/${DRAM_MODEL}.asc ${TMP_MODEL_DIR}/${DM}.asc && \
${SPICE_BIN} -m ${TMP_MODEL_DIR}/${DM}.asc -o ${OUT_DIR} ${EXTRA_PARAMS} &> /dev/null && \
${SPARSER_BIN} ${OUT_DIR}/${DM}.raw ${EXP_VAL} ${VDD} > ${OUT_DIR}/${DM}.out &
pids="$pids $!"

# Run three-cell sim
MODEL_SUFFIX=_3c
CONFIGS_T=$CONFIGS" wl0_act=1.0 wl1_act=1.0 wl2_act=1.0 wl3_act=0
                  wl4_act=0 wl5_act=0 wl6_act=0 wl7_act=0"
CF=${CONFIG_FILE}${MODEL_SUFFIX}
EXTRA_PARAMS="-f ${CF}"
${GEN_CONFIG} ${CF} ${CONFIGS_T}

DM=${DRAM_MODEL}${MODEL_SUFFIX}

cp ${DRAM_MODEL_DIR}/${DRAM_MODEL}.asc ${TMP_MODEL_DIR}/${DM}.asc && \
${SPICE_BIN} -m ${TMP_MODEL_DIR}/${DM}.asc -o ${OUT_DIR} ${EXTRA_PARAMS} &> /dev/null && \
${SPARSER_BIN} ${OUT_DIR}/${DM}.raw ${EXP_VAL} ${VDD} > ${OUT_DIR}/${DM}.out &
pids="$pids $!"

# Run four-cell sim
MODEL_SUFFIX=_4c
CONFIGS_T=$CONFIGS" wl0_act=1.0 wl1_act=1.0 wl2_act=1.0 wl3_act=1.0
                  wl4_act=0 wl5_act=0 wl6_act=0 wl7_act=0"
CF=${CONFIG_FILE}${MODEL_SUFFIX}
EXTRA_PARAMS="-f ${CF}"
${GEN_CONFIG} ${CF} ${CONFIGS_T}

DM=${DRAM_MODEL}${MODEL_SUFFIX}

cp ${DRAM_MODEL_DIR}/${DRAM_MODEL}.asc ${TMP_MODEL_DIR}/${DM}.asc && \
${SPICE_BIN} -m ${TMP_MODEL_DIR}/${DM}.asc -o ${OUT_DIR} ${EXTRA_PARAMS} &> /dev/null && \
${SPARSER_BIN} ${OUT_DIR}/${DM}.raw ${EXP_VAL} ${VDD} > ${OUT_DIR}/${DM}.out &
pids="$pids $!"

# Run 5-cell sim
MODEL_SUFFIX=_5c
CONFIGS_T=$CONFIGS" wl0_act=1.0 wl1_act=1.0 wl2_act=1.0 wl3_act=1.0
                  wl4_act=1.0 wl5_act=0 wl6_act=0 wl7_act=0"
CF=${CONFIG_FILE}${MODEL_SUFFIX}
EXTRA_PARAMS="-f ${CF}"
${GEN_CONFIG} ${CF} ${CONFIGS_T}

DM=${DRAM_MODEL}${MODEL_SUFFIX}

cp ${DRAM_MODEL_DIR}/${DRAM_MODEL}.asc ${TMP_MODEL_DIR}/${DM}.asc && \
${SPICE_BIN} -m ${TMP_MODEL_DIR}/${DM}.asc -o ${OUT_DIR} ${EXTRA_PARAMS} &> /dev/null && \
${SPARSER_BIN} ${OUT_DIR}/${DM}.raw ${EXP_VAL} ${VDD} > ${OUT_DIR}/${DM}.out &
pids="$pids $!"

# Run 6-cell sim
MODEL_SUFFIX=_6c
CONFIGS_T=$CONFIGS" wl0_act=1.0 wl1_act=1.0 wl2_act=1.0 wl3_act=1.0
                  wl4_act=1.0 wl5_act=1.0 wl6_act=0 wl7_act=0"
CF=${CONFIG_FILE}${MODEL_SUFFIX}
EXTRA_PARAMS="-f ${CF}"
${GEN_CONFIG} ${CF} ${CONFIGS_T}

DM=${DRAM_MODEL}${MODEL_SUFFIX}

cp ${DRAM_MODEL_DIR}/${DRAM_MODEL}.asc ${TMP_MODEL_DIR}/${DM}.asc && \
${SPICE_BIN} -m ${TMP_MODEL_DIR}/${DM}.asc -o ${OUT_DIR} ${EXTRA_PARAMS} &> /dev/null && \
${SPARSER_BIN} ${OUT_DIR}/${DM}.raw ${EXP_VAL} ${VDD} > ${OUT_DIR}/${DM}.out &
pids="$pids $!"

# Run 7-cell sim
MODEL_SUFFIX=_7c
CONFIGS_T=$CONFIGS" wl0_act=1.0 wl1_act=1.0 wl2_act=1.0 wl3_act=1.0
                  wl4_act=1.0 wl5_act=1.0 wl6_act=1.0 wl7_act=0"
CF=${CONFIG_FILE}${MODEL_SUFFIX}
EXTRA_PARAMS="-f ${CF}"
${GEN_CONFIG} ${CF} ${CONFIGS_T}

DM=${DRAM_MODEL}${MODEL_SUFFIX}

cp ${DRAM_MODEL_DIR}/${DRAM_MODEL}.asc ${TMP_MODEL_DIR}/${DM}.asc && \
${SPICE_BIN} -m ${TMP_MODEL_DIR}/${DM}.asc -o ${OUT_DIR} ${EXTRA_PARAMS} &> /dev/null && \
${SPARSER_BIN} ${OUT_DIR}/${DM}.raw ${EXP_VAL} ${VDD} > ${OUT_DIR}/${DM}.out &
pids="$pids $!"

# Run 8-cell sim
MODEL_SUFFIX=_8c
CONFIGS_T=$CONFIGS" wl0_act=1.0 wl1_act=1.0 wl2_act=1.0 wl3_act=1.0
                  wl4_act=1.0 wl5_act=1.0 wl6_act=1.0 wl7_act=1.0"
CF=${CONFIG_FILE}${MODEL_SUFFIX}
EXTRA_PARAMS="-f ${CF}"
${GEN_CONFIG} ${CF} ${CONFIGS_T}

DM=${DRAM_MODEL}${MODEL_SUFFIX}

cp ${DRAM_MODEL_DIR}/${DRAM_MODEL}.asc ${TMP_MODEL_DIR}/${DM}.asc && \
${SPICE_BIN} -m ${TMP_MODEL_DIR}/${DM}.asc -o ${OUT_DIR} ${EXTRA_PARAMS} &> /dev/null && \
${SPARSER_BIN} ${OUT_DIR}/${DM}.raw ${EXP_VAL} ${VDD} > ${OUT_DIR}/${DM}.out &
pids="$pids $!"

wait $pids

rm -r -f ${TMP_MODEL_DIR}
