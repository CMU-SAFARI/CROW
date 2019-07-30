#!/bin/bash

CONFIG_FILE=$1
shift

rm -f ${CONFIG_FILE}

for var in "$@"
do
    param=(${var//=/ })
    echo "set ${param[@]}" >> ${CONFIG_FILE}
done
