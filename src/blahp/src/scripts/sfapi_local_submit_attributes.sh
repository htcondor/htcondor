#!/bin/bash

strip_quotes() {
    echo ${1//\"/}
}

if [ -n "${NODES}" ]; then
    nodes="#SBATCH  --nodes=$(strip_quotes $NODES)"
    echo $nodes
fi

if [ -n "${CORES}" ]; then
    echo "#SBATCH --ntasks=$(strip_quotes $CORES)"
fi

if [ -n "${GPUS}" ]; then
    echo "#SBATCH --gpus=$(strip_quotes $GPUS)"
fi

if [ -n "${WALLTIME}" ]; then
    echo "#SBATCH --time=$(strip_quotes $WALLTIME)"
fi

if [ -n "${PER_PROCESS_MEMORY}" ]; then
    echo "#SBATCH --mem-per-cpu=$(strip_quotes $PER_PROCESS_MEMORY)"
fi

if [ -n "${TOTAL_MEMORY}" ]; then
    echo "#SBATCH --mem=$(strip_quotes $TOTAL_MEMORY)"
fi

if [ -n "${JOBNAME}" ]; then
    echo "#SBATCH --job-name ${JOBNAME}"
fi

if [ -n "${PROJECT}" ]; then
    echo "#SBATCH --account ${PROJECT}"
fi

# if a user passed any extra arguments set them in the end
# for example "-N testjob -l walltime=01:23:45 -l nodes=2"
if [ -n "${EXTRA_ARGUMENTS}" ]; then
    echo "#SBATCH $(strip_quotes "$EXTRA_ARGUMENTS")"
fi
