#!/bin/bash

# This file is sourced by blahp before submitting the job to Flux
# Anything printed to stdout is included in the submit file.
# For example, to set a default walltime of 24 hours in Flux, you
# could uncomment this line:

# echo "#FLUX: --time-limit=24h"

# blahp allows arbitrary attributes to be passed to this script on a per-job
# basis.  If you add the following to your HTCondor-G submit file:

#+remote_cerequirements = NumJobs == 100 && foo = 5

# Then an environment variable, NumJobs, will be exported prior to calling this
# script and set to a value of 100.  The variable foo will be set to 5.

# You could allow users to set the walltime for the job with the following
# customization (flux syntax given; adjust for the appropriate batch system):

# Uncomment the else block to default to 24 hours of runtime; otherwise, the queue
# default is used.
if [ -n "$Walltime" ]; then
    let Walltime=Walltime/60
    echo "#FLUX: --time-limit=${Walltime}m"
# else
#     echo "#FLUX: --time-limit=24h"
fi

# Uncomment the block below to provide several other useful settings

# strip_quotes() {
#     echo ${1//\"/}
# }
# 
# if [ -n "${NODES}" ]; then
#     nodes="#FLUX: --nodes=$(strip_quotes $NODES)"
#     echo $nodes
# fi
# 
# if [ -n "${CORES}" ] && [ ! -n "${GPUS}" ]; then
#     echo "#FLUX: --nslots=$(strip_quotes $CORES)"
#     echo "#FLUX: --cores-per-slot=1"
# elif [ ! -n "${CORES}" ] && [ -n "${GPUS}" ]; then
#     echo "#FLUX: --nslots=$(strip_quotes $GPUS)"
#     echo "#FLUX: --gpus-per-slot=1"
# elif [ -n "${CORES}" ] && [ -n "${GPUS}" ]; then
#     if [ "${CORES}" -gt "${GPUS}" ]; then
#         echo "#FLUX: --nslots=$(strip_quotes $CORES)"
#         echo "#FLUX: --cores-per-slot=1"
#         echo "#FLUX: --gpus-per-slot=1"
#     else
#         echo "#FLUX: --nslots=$(strip_quotes $GPUS)"
#         echo "#FLUX: --cores-per-slot=1"
#         echo "#FLUX: --gpus-per-slot=1"
#     fi
# fi
# 
# # Add if/when Flux allows users to specify memory
# # if [ -n "${PER_PROCESS_MEMORY}" ]; then
# #     echo "#SBATCH --mem-per-cpu=$(strip_quotes $PER_PROCESS_MEMORY)"
# # fi
# 
# # Add if/when Flux allows users to specify memory
# # if [ -n "${TOTAL_MEMORY}" ]; then
# #     echo "#SBATCH --mem=$(strip_quotes $TOTAL_MEMORY)"
# # fi
# 
# if [ -n "${JOBNAME}" ]; then
#     echo "#FLUX: --job-name=${JOBNAME}"
# fi
# 
# if [ -n "${PROJECT}" ]; then
#     echo "#FLUX: --bank=${PROJECT}"
# fi
# 
# # if a user passed any extra arguments set them in the end
# # for example "-N testjob -l walltime=01:23:45 -l nodes=2"
# if [ -n "${EXTRA_ARGUMENTS}" ]; then
#     echo "#FLUX: $(strip_quotes "$EXTRA_ARGUMENTS")"
# fi

