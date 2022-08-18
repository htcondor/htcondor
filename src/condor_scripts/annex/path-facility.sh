#!/bin/bash

CONTROL_PREFIX="=-.-="
echo "${CONTROL_PREFIX} PID $$"

function usage() {
    echo "Usage: ${0} \\"
    echo "       JOB_NAME QUEUE_NAME COLLECTOR TOKEN_FILE LIFETIME PILOT_BIN \\"
    echo "       OWNERS NODES MULTI_PILOT_BIN ALLOCATION REQUEST_ID PASSWORD_FILE \\"
    echo "       [CPUS] [MEM_MB]"
    echo "where OWNERS is a comma-separated list.  Omit CPUS and MEM_MB to get"
    echo "whole-node jobs.  NODES is ignored on non-whole-node jobs."
}

JOB_NAME=$1
if [[ -z $JOB_NAME ]]; then
    usage
    exit 1
fi

QUEUE_NAME=$2
if [[ -z $QUEUE_NAME ]]; then
    usage
    exit 1
fi

COLLECTOR=$3
if [[ -z $COLLECTOR ]]; then
    usage
    exit 1
fi

TOKEN_FILE=$4
if [[ -z $TOKEN_FILE ]]; then
    usage
    exit 1
fi

LIFETIME=$5
if [[ -z $LIFETIME ]]; then
    usage
    exit 1
fi

PILOT_BIN=$6
if [[ -z $PILOT_BIN ]]; then
    usage
    exit 1
fi

OWNERS=$7
if [[ -z $OWNERS ]]; then
    usage
    exit 1
fi

NODES=$8
if [[ -z $NODES ]]; then
    usage
    exit 1
fi

MULTI_PILOT_BIN=$9
if [[ -z $MULTI_PILOT_BIN ]]; then
    usage
    exit 1
fi

ALLOCATION=${10}
if [[ $ALLOCATION == "None" ]]; then
    ALLOCATION=""
fi
if [[ -z $ALLOCATION ]]; then
    echo "Will try to use the default allocation."
fi

REQUEST_ID=${11}
if [[ -z $REQUEST_ID ]]; then
    usage
    exit 1
fi

PASSWORD_FILE=${12}
if [[ -z $PASSWORD_FILE ]]; then
    usage
    exit 1
fi


#
# Site-specific defaults.  Should probably be set in the Python front-end
# code, but we don't do that yet.
#

CPUS=${13}
if [[ $CPUS == "None" ]]; then
    CPUS="1"
fi

MEM_MB=${14}
if [[ $MEM_MB == "None" ]]; then
    MEM_MB="2048"
fi


#
# Constants.
#


# The binaries must be a tarball named condor-*, and unpacking that tarball
# must create a directory which also matches condor-*.
WELL_KNOWN_LOCATION_FOR_BINARIES=https://research.cs.wisc.edu/htcondor/tarball/current/9.5.4/update/condor-9.5.4-20220207-x86_64_Rocky8-stripped.tar.gz

# The configuration must be a tarball which does NOT match condor-*.  It
# will be unpacked in the root of the directory created by unpacking the
# binaries and as such should contain files in local/config.d/*.
WELL_KNOWN_LOCATION_FOR_CONFIGURATION=https://cs.wisc.edu/~tlmiller/hpc-config.tar.gz

# How early should HTCondor exit to make sure we have time to clean up?
CLEAN_UP_TIME=300


#
# Create a temporary directory.
#
echo -e "\rStep 1 of 8: Creating temporary directory...."
SCRATCH=${SCRATCH:-~/.annex-scratch}
mkdir -p "$SCRATCH"
PILOT_DIR=`/usr/bin/mktemp --directory --tmpdir=${SCRATCH} pilot.XXXXXXXX 2>&1`
if [[ $? != 0 ]]; then
    echo "Failed to create temporary directory for pilot, aborting."
    echo ${PILOT_DIR}
    exit 1
fi
echo "${CONTROL_PREFIX} PILOT_DIR ${PILOT_DIR}"

function cleanup() {
    echo "Cleaning up temporary directory..."
    rm -fr ${PILOT_DIR}
}
trap cleanup EXIT


#
# The SLURM scripts all assume a shared filesystem between the SSH-target
# node and the execute nodes.  The PATh facility doesn't have one, so we
# construct the pilot directory in the pilot job, instead.
#
# Likewise, we don't pre-stage .sif files for the PATh facility.
#
# Therefore, all we need to do here is make sure that the token and password
# files exist when the pilot job starts.
#
# FIXME: ... and to garbage-collect old ones.  (Delete the whole pilot
# directory if the token has expired.)
#

mv ${PILOT_BIN} ${PILOT_DIR}/path-facility.pilot
mv ${TOKEN_FILE} ${PILOT_DIR}/token_file
mv ${PASSWORD_FILE} ${PILOT_DIR}/password_file


#
# Write the HTCONDOR job.
#

ALLOCATION_LINE="# Trying to use default allocation"
if [[ -n $ALLOCATION ]]; then
    ALLOCATION_LINE="account_group = ${ALLOCATION}"
fi

echo "

universe                    = vanilla
batch_name                  = ${JOB_NAME}

executable                  = path-facility.pilot
transfer_executable         = true
arguments                   = ${JOB_NAME} ${COLLECTOR} ${LIFETIME} ${OWNERS} ${REQUEST_ID} \$(ClusterID)_\$(ProcID) ${CPUS} ${MEM_MB}

transfer_input_files        = ${WELL_KNOWN_LOCATION_FOR_BINARIES}, ${WELL_KNOWN_LOCATION_FOR_CONFIGURATION}, token_file, password_file
transfer_output_files       = \"\"

output                      = out.\$(ClusterID).\$(ProcID)
error                       = err.\$(ClusterID).\$(ProcID)
log                         = log.\$(ClusterID).\$(ProcID)

# The WISC-PATH site runs CentOS 7.
requirements                = GLIDEIN_Site == \"UNL-PATH\"

${ALLOCATION_LINE}

# This is a bit under the average amount of disk per core in the facility.
request_disk                = \$(request_cpus) * 15000 * 1024

request_cpus                = ${CPUS}
request_memory              = ${MEM_MB}

queue ${NODES}
" > ${PILOT_DIR}/path-facility.submit

#
# Submit the HTCondor job.
#
echo -e "\rStep 8 of 8: Submitting HTCondor job........."
SUBMIT_LOG=${PILOT_DIR}/submit.log
cd ${PILOT_DIR}; condor_submit ./path-facility.submit &> ${SUBMIT_LOG}
SUBMIT_ERROR=$?
if [[ $SUBMIT_ERROR != 0 ]]; then
    echo "Failed to submit job to HTCondor (${SUBMIT_ERROR}), aborting."
    cat ${SUBMIT_LOG}
    exit 6
fi
JOB_ID="FIXME"
echo "${CONTROL_PREFIX} JOB_ID ${JOB_ID}"
echo ""

# Reset the EXIT trap so that we don't delete the temporary directory
# that the HTCondor job needs.  (We pass it the temporary directory so
# that it can clean up after itself.)
trap EXIT
exit 0
