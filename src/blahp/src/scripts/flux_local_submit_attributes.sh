#!/bin/bash

strip_quotes() {
    echo ${1//\"/}
}

if [ -n "${NODES}" ]; then
    nodes="#FLUX: --nodes=$(strip_quotes $NODES)"
    echo $nodes
fi

if [ -n "${CORES}" ] && [ ! -n "${GPUS}" ]; then
    echo "#FLUX: --nslots=$(strip_quotes $CORES)"
    echo "#FLUX: --cores-per-slot=1"
elif [ ! -n "${CORES}" ] && [ -n "${GPUS}" ]; then
    echo "#FLUX: --nslots=$(strip_quotes $GPUS)"
    echo "#FLUX: --gpus-per-slot=1"
elif [ -n "${CORES}" ] && [ -n "${GPUS}" ]; then
    if [ "${CORES}" -gt "${GPUS}" ]; then
        echo "#FLUX: --nslots=$(strip_quotes $CORES)"
        echo "#FLUX: --cores-per-slot=1"
        echo "#FLUX: --gpus-per-slot=1"
    else
        echo "#FLUX: --nslots=$(strip_quotes $GPUS)"
        echo "#FLUX: --cores-per-slot=1"
        echo "#FLUX: --gpus-per-slot=1"
    fi
fi

if [ -n "${WALLTIME}" ]; then
    echo "#FLUX: --time-limit=$(strip_quotes $WALLTIME)"
fi

# Add if/when Flux allows users to specify memory
# if [ -n "${PER_PROCESS_MEMORY}" ]; then
#     echo "#SBATCH --mem-per-cpu=$(strip_quotes $PER_PROCESS_MEMORY)"
# fi

# Add if/when Flux allows users to specify memory
# if [ -n "${TOTAL_MEMORY}" ]; then
#     echo "#SBATCH --mem=$(strip_quotes $TOTAL_MEMORY)"
# fi

if [ -n "${JOBNAME}" ]; then
    echo "#FLUX: --job-name=${JOBNAME}"
fi

if [ -n "${PROJECT}" ]; then
    echo "#FLUX: --bank=${PROJECT}"
fi

# if a user passed any extra arguments set them in the end
# for example "-N testjob -l walltime=01:23:45 -l nodes=2"
if [ -n "${EXTRA_ARGUMENTS}" ]; then
    echo "#FLUX: $(strip_quotes "$EXTRA_ARGUMENTS")"
fi

