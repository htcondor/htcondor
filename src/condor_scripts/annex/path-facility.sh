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

CPUS=${13}
if [[ $CPUS == "None" ]]; then
    CPUS=""
fi

MEM_MB=${14}
if [[ $MEM_MB == "None" ]]; then
    MEM_MB=""
fi

BIRTH=`date +%s`
# echo "Starting script at `date`..."

#
# Download and configure the pilot on the head node before running it
# on the execute node(s).
#
# The following variables are constants.
#


# The binaries must be a tarball named condor-*, and unpacking that tarball
# must create a directory which also matches condor-*.
WELL_KNOWN_LOCATION_FOR_BINARIES=https://research.cs.wisc.edu/htcondor/tarball/current/9.5.4/update/condor-9.5.4-20220207-x86_64_Rocky8-stripped.tar.gz

# The configuration must be a tarball which does NOT match condor-*.  It
# will be unpacked in the root of the directory created by unpacking the
# binaries and as such should contain files in local/config.d/*.
WELL_KNOWN_LOCATION_FOR_CONFIGURATION=https://cs.wisc.edu/~tlmiller/hpc-config.tar.gz

# FIXME: do we actually need this if we know we're running under HTCONDOR?
# How early should HTCondor exit to make sure we have time to clean up?
CLEAN_UP_TIME=300

echo -e "\rStep 1 of 8: Creating temporary directory...."
SCRATCH=${SCRATCH:-~/.annex-scratch}
mkdir -p "$SCRATCH"
PARENT_DIR=`/usr/bin/mktemp --directory --tmpdir=${SCRATCH} pilot.XXXXXXXX 2>&1`
if [[ $? != 0 ]]; then
    echo "Failed to create parent directory for pilot, aborting."
    echo ${PARENT_DIR}
    exit 1
fi
PILOT_DIR=${PARENT_DIR}/pilot
mkdir -p ${PILOT_DIR}
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
# trap cleanup EXIT

#
# Download the configuration.  (Should be smaller, and we fail if either
# of these downloads fail, so we may as well try this one first.)
#

cd ${PILOT_DIR}

# The .sif files need to live in ${PILOT_DIR} for the same reason.  We
# require that they have been transferred to the same directory as the
# PILOT_BIN mainly because this script has too many arguments already.
SIF_DIR=${PILOT_DIR}/sif
mkdir ${SIF_DIR}
PILOT_BIN_DIR=`dirname ${PILOT_BIN}`
mv ${PILOT_BIN_DIR}/sif ${PILOT_DIR}

# The pilot scripts need to live in the ${PILOT_DIR} because the front-end
# copied them into a temporary directory that it's responsible for cleaning up.
mv ${PILOT_BIN} ${PILOT_DIR}
mv ${MULTI_PILOT_BIN} ${PILOT_DIR}
PILOT_BIN=${PILOT_DIR}/`basename ${PILOT_BIN}`
MULTI_PILOT_BIN=${PILOT_DIR}/`basename ${MULTI_PILOT_BIN}`

echo -e "\rStep 2 of 8: downloading configuration......."
CONFIGURATION_FILE=`basename ${WELL_KNOWN_LOCATION_FOR_CONFIGURATION}`
CURL_LOGGING=`curl -fsSL ${WELL_KNOWN_LOCATION_FOR_CONFIGURATION} -o ${CONFIGURATION_FILE} 2>&1`
if [[ $? != 0 ]]; then
    echo "Failed to download configuration from '${WELL_KNOWN_LOCATION_FOR_CONFIGURATION}', aborting."
    echo ${CURL_LOGGING}
    exit 2
fi

#
# Download the binaries.
#
echo -e "\rStep 3 of 8: downloading required software..."
BINARIES_FILE=`basename ${WELL_KNOWN_LOCATION_FOR_BINARIES}`
CURL_LOGGING=`curl -fsSL ${WELL_KNOWN_LOCATION_FOR_BINARIES} -o ${BINARIES_FILE} 2>&1`
if [[ $? != 0 ]]; then
    echo "Failed to download configuration from '${WELL_KNOWN_LOCATION_FOR_BINARIES}', aborting."
    echo ${CURL_LOGGING}
    exit 2
fi

#
# Unpack the binaries.
#
echo -e "\rStep 4 of 8: unpacking software.............."
TAR_LOGGING=`tar -z -x -f ${BINARIES_FILE} 2>&1`
if [[ $? != 0 ]]; then
    echo "Failed to unpack binaries from '${BINARIES_FILE}', aborting."
    echo ${TAR_LOGGING}
    exit 3
fi

#
# Make the personal condor.
#
rm condor-*.tar.gz
cd condor-*

echo -e "\rStep 5 of 8: configuring software (part 1)..."
MPC_LOGGING=`./bin/make-personal-from-tarball 2>&1`
if [[ $? != 0 ]]; then
    echo "Failed to make personal condor, aborting."
    echo ${MPC_LOGGING}
    exit 4
fi


# It may have take some time to get everything installed, so to make sure
# we get our full clean-up time, subtract off how long we've been running
# already.
YOUTH=$((`date +%s` - ${BIRTH}))
REMAINING_LIFETIME=$(((${LIFETIME} - ${YOUTH}) - ${CLEAN_UP_TIME}))

echo -e "\rStep 6 of 8: configuring software (part 2)..."
rm local/config.d/00-personal-condor
echo "
use role:execute
use security:recommended_v9_0
use feature:PartitionableSLot

COLLECTOR_HOST = ${COLLECTOR}

# We shouldn't ever actually need this, but it's convenient for testing.
SHARED_PORT_PORT = 0

# Allows condor_off (et alia) to work from the head node.
ALLOW_ADMINISTRATOR = \$(ALLOW_ADMINISTRATOR) $(whoami)@$(hostname)

# FIXME: use same-AuthenticatedIdentity once that becomes available, instead.
# Allows condor_off (et alia) to work from the submit node.
ALLOW_ADMINISTRATOR = \$(ALLOW_ADMINISTRATOR) condor_pool@*
SEC_DEFAULT_AUTHENTICATION_METHODS = FS IDTOKENS PASSWORD

# Eliminate a bogus, repeated warning in the logs.  This is a bug;
# it should be the default.
SEC_PASSWORD_DIRECTORY = \$(LOCAL_DIR)/passwords.d

# This is a bug; it should be the default.
SEC_TOKEN_SYSTEM_DIRECTORY = \$(LOCAL_DIR)/tokens.d
# Having to set it twice is also a bug.
SEC_TOKEN_DIRECTORY = \$(LOCAL_DIR)/tokens.d

# Don't run benchmarks.
RUNBENCHMARKS = FALSE

# We definitely need CCB.
CCB_ADDRESS = \$(COLLECTOR_HOST)

#
# Commit suicide after being idle for five minutes.
#
STARTD_NOCLAIM_SHUTDOWN = 300

#
# Don't run for more than two hours, to make sure we have time to clean up.
#
MASTER.DAEMON_SHUTDOWN_FAST = (CurrentTime - DaemonStartTime) > ${REMAINING_LIFETIME}

# Only start jobs from the specified owner.
START = \$(START) && stringListMember( Owner, \"${OWNERS}\" )

# Advertise the standard annex attributes (master ad for condor_off).
IsAnnex = TRUE
AnnexName = \"${JOB_NAME}\"
hpc_annex_request_id = \"${REQUEST_ID}\"
STARTD_ATTRS = \$(STARTD_ATTRS) AnnexName IsAnnex hpc_annex_request_id
MASTER_ATTRS = \$(MASTER_ATTRS) AnnexName IsAnnex hpc_annex_request_id

# Force all container-universe jobs to try to use pre-staged .sif files.
# This should be removed when we handle this in HTCondor proper.
JOB_EXECUTION_TRANSFORM_NAMES = siffile
JOB_EXECUTION_TRANSFORM_siffile @=end
if defined MY.ContainerImage
    EVALSET ContainerImage strcat(\"${SIF_DIR}/\", MY.ContainerImage)
endif
@end

#
# Subsequent configuration is machine-specific.
#

" > local/config.d/00-basic-pilot

mkdir local/passwords.d
mkdir local/tokens.d
mv ${TOKEN_FILE} local/tokens.d
mv ${PASSWORD_FILE} local/passwords.d/POOL

#
# Unpack the configuration on top.
#

echo -e "\rStep 7 of 8: configuring software (part 3)..."
TAR_LOGGING=`tar -z -x -f ../${CONFIGURATION_FILE} 2>&1`
if [[ $? != 0 ]]; then
    echo "Failed to unpack binaries from '${CONFIGURATION_FILE}', aborting."
    echo ${TAR_LOGGING}
    exit 5
fi

# Compute the appropriate duration. (-t)
#
# This script does NOT embed knowledge about this machine's queue limits.  It
# seems like it'll be much easier to embed that knowledge in the UI script
# (rather than transmit a reasonable error back), plus it'll be more user-
# friendly, since they won't have to log in to get error about requesting
# the wrong queue length.
MINUTES=$(((${REMAINING_LIFETIME} + ${CLEAN_UP_TIME})/60))

#
# HTCondor doesn't like transferring symlinks to directories, so
# we have to tar everything back up again.  *sigh*
#
cd ${PARENT_DIR}

TAR_LOGGING=`tar -c -z -f pilot.tar.gz pilot 2>&1`
if [[ $? != 0 ]]; then
    echo "Failed to repack pilot, aborting."
    echo ${TAR_LOGGING}
    exit 6
fi

cp ${PILOT_DIR}/path-facility.pilot ${PARENT_DIR}

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
transfer_executable  = true
transfer_input_files        = pilot.tar.gz
# Request no output back.
transfer_output_files       = ""

output                      = ${PARENT_DIR}/out.\$(ClusterID).\$(ProcID)
error                       = ${PARENT_DIR}/err.\$(ClusterID).\$(ProcID)
log                         = ${PARENT_DIR}/log.\$(ClusterID).\$(ProcID)

# FIXME
# request_cpu                 = ${CPUS}
# request_memory              = ${MEM_MB}
request_cpus                = 1
request_memory              = 8096
# FIXME
request_disk                = 8GB

# FIXME HARDER
stream_error                = true
stream_output               = true

${ALLOCATION_LINE}

queue 1
" > ${PARENT_DIR}/path-facility.submit

#
# Submit the HTCondor job.
#
echo -e "\rStep 8 of 8: Submitting HTCondor job............"
SUBMIT_LOG=${PILOT_DIR}/submit.log
condor_submit ${PARENT_DIR}/path-facility.submit &> ${SUBMIT_LOG}
SUBMIT_ERROR=$?
if [[ $SUBMIT_ERROR != 0 ]]; then
    echo "Failed to submit job to HTCondor (${SUBMIT_ERROR}), aborting."
    cat ${SUBMIT_LOG}
    exit 6
fi
JOB_ID=``
echo "${CONTROL_PREFIX} JOB_ID ${JOB_ID}"
echo ""

# Reset the EXIT trap so that we don't delete the temporary directory
# that the HTCondor job needs.  (We pass it the temporary directory so
# that it can clean up after itself.)
trap EXIT
exit 0
