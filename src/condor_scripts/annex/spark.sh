#!/bin/bash

CONTROL_PREFIX="=-.-="
echo "${CONTROL_PREFIX} PID $$"

function usage() {
    echo "Usage: ${0} SYSTEM STARTD_NOCLAIM_SHUTDOWN\\"
    echo "       JOB_NAME QUEUE_NAME COLLECTOR TOKEN_FILE LIFETIME PILOT_BIN \\"
    echo "       OWNERS NODES MULTI_PILOT_BIN ALLOCATION REQUEST_ID \\"
    echo "       PASSWORD_FILE SCHEDD_NAME \\"
    echo "       [CPUS] [MEM_MB] [GPUS]"
    echo "where OWNERS is a comma-separated list.  Omit CPUS and MEM_MB to get"
    echo "whole-node jobs.  NODES is ignored on non-whole-node jobs."
}

SYSTEM=$1
if [[ -z $SYSTEM ]]; then
    usage
    exit 1
fi
shift

STARTD_NOCLAIM_SHUTDOWN=$1
if [[ -z $STARTD_NOCLAIM_SHUTDOWN ]]; then
    usage
    exit 1
fi
shift

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

SCHEDD_NAME=${13}
if [[ -z $SCHEDD_NAME ]]; then
    usage
    exit 1
fi

CPUS=${14}
if [[ $CPUS == "None" ]]; then
    CPUS=""
fi

MEM_MB=${15}
if [[ $MEM_MB == "None" ]]; then
    MEM_MB=""
fi

GPUS=${16}
if [[ $GPUS == "None" ]]; then
    GPUS=""
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
WELL_KNOWN_LOCATION_FOR_BINARIES=https://research.cs.wisc.edu/htcondor/tarball/23.x/23.8.1/release/condor-23.8.1-x86_64_AlmaLinux8-stripped.tar.gz

# The configuration must be a tarball which does NOT match condor-*.  It
# will be unpacked in the root of the directory created by unpacking the
# binaries and as such should contain files in local/config.d/*.
WELL_KNOWN_LOCATION_FOR_CONFIGURATION=https://cs.wisc.edu/~tlmiller/hpc-config.tar.gz

# How early should HTCondor exit to make sure we have time to clean up?
CLEAN_UP_TIME=300


#
# Pulls in the system-specific variables SCRATCH, CONFIG_FRAGMENT,
# and SHELL_FRAGMENT.
#
MEMORY_CHUNK_SIZE=3072
PILOT_BIN_DIR=`dirname "${PILOT_BIN}"`
SBATCH_CONSTRAINT_LINE=""
. "${PILOT_BIN_DIR}/${SYSTEM}.fragment"

if [[ -z $SCRATCH ]]; then
    echo "Internal error: SCRATCH not set after loading system-specific fragment."
    exit 7
fi

#
# Create pilot-specific directory on shared storage.  The least-awful way
# to do this is by having the per-node script NOT exec condor_master, but
# instead configure the condor_master to exit well before the "run time"
# of the job, and the script carry on to do the clean-up.
#
# That won't work for multi-node jobs, which we'll need eventually, but
# we'll leave that for then.
#


echo -e "\rStep 1 of 8: Creating temporary directory...."
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
# Download the configuration.  (Should be smaller, and we fail if either
# of these downloads fail, so we may as well try this one first.)
#

cd ${PILOT_DIR}

# The .sif files need to live in ${PILOT_DIR} for the same reason.  We
# require that they have been transferred to the same directory as the
# PILOT_BIN mainly because this script has too many arguments already.
SIF_DIR=${PILOT_DIR}/sif
mkdir ${SIF_DIR}
if [[ -d ${PILOT_BIN_DIR}/sif ]]; then
    mv ${PILOT_BIN_DIR}/sif ${PILOT_DIR}
fi

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
    echo "Failed to download binaries from '${WELL_KNOWN_LOCATION_FOR_BINARIES}', aborting."
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

SHELL_FRAGMENT

# It may have take some time to get everything installed, so to make sure
# we get our full clean-up time, subtract off how long we've been running
# already.
YOUTH=$((`date +%s` - ${BIRTH}))
REMAINING_LIFETIME=$(((${LIFETIME} - ${YOUTH}) - ${CLEAN_UP_TIME}))


WHOLE_NODE=1
CONDOR_CPUS_LINE=""
if [[ -n $CPUS && $CPUS -gt 0 ]]; then
    CONDOR_CPUS_LINE="NUM_CPUS = ${CPUS}"
    WHOLE_NODE=""
fi

CONDOR_MEMORY_LINE=""
if [[ -n $MEM_MB && $MEM_MB -gt 0 ]]; then
    CONDOR_MEMORY_LINE="MEMORY = ${MEM_MB}"
    WHOLE_NODE=""
fi

CONDOR_GPUS_LINE=""
if [[ $GPUS ]]; then
    CONDOR_GPUS_LINE="use feature:gpus"
fi

# Using <${COLLECTOR}> is a hack, but easier than figuring out the quoting.
echo -e "\rStep 6 of 8: configuring software (part 2)..."
rm local/config.d/00-personal-condor
echo "
use role:execute
use security:recommended
use feature:PartitionableSLot

COLLECTOR_HOST = <${COLLECTOR}>

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
CCB_ADDRESS = ${COLLECTOR}

#
# Commit suicide after being idle for long enough.
#
STARTD_NOCLAIM_SHUTDOWN = ${STARTD_NOCLAIM_SHUTDOWN}

#
# Send updates a bit more than twice as often as the default, since this
# is an ephemeral resource.  If STARTD_NOCLAIM_SHUTDOWN is the default,
# this gives the startd a second chance to match if its initial update
# to the collector failed.
#
UPDATE_INTERVAL = 137


#
# Don't run for more than two hours, to make sure we have time to clean up.
#
MASTER.DAEMON_SHUTDOWN_FAST = (CurrentTime - DaemonStartTime) > ${REMAINING_LIFETIME}

# Only start jobs from the specified owner.
START = \$(START) && stringListMember( Owner, \"${OWNERS}\" )
# Only start jobs for this annex.
START = \$(START) && MY.AnnexName == TARGET.TargetAnnexName

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
    EVALMACRO FOO=TransferContainer ?: true
    if \$(FOO)
        EVALSET ContainerImage strcat(\"${SIF_DIR}/\", MY.ContainerImage)
        SET TransferContainer false
    endif
endif
@end

#
# Connect directly to the schedd.
#
STARTD_DIRECT_ATTACH_SCHEDD_NAME = ${SCHEDD_NAME}
STARTD_DIRECT_ATTACH_SCHEDD_POOL = ${COLLECTOR}

# Update slots often enough that we can do more than one job. ;)
# The direct attach interval is the minimum interval; this interval
# is checked every UPDATE_INTERVAL.
STARTD_DIRECT_ATTACH_INTERVAL = 5
UPDATE_INTERVAL = 5

#
# Subsequent configuration is machine-specific.
#

${CONDOR_CPUS_LINE}
${CONDOR_MEMORY_LINE}
${CONDOR_GPUS_LINE}

#
# We have to hand out memory in chunks large enough to avoid the problem(s)
# HTCondor has with having too many slots.  The appropriate chunk size varies
# from system to system.
#
MUST_MODIFY_REQUEST_EXPRS = TRUE
MODIFY_REQUEST_EXPR_REQUESTMEMORY = max({ ${MEMORY_CHUNK_SIZE}, quantize(RequestMemory, {128}) })

$(CONFIG_FRAGMENT)

" > local/config.d/00-basic-pilot

mkdir -p local/passwords.d
mkdir -p local/tokens.d
mv ${TOKEN_FILE} local/tokens.d
# On Delta, if this pair is just `mv`, instead, there's a weird warning.
cp ${PASSWORD_FILE} local/passwords.d/POOL
rm -f ${PASSWORD_FILE}


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

#
# Write the SLURM job.
#

# Compute the appropriate duration. (-t)
#
# This script does NOT embed knowledge about this machine's queue limits.  It
# seems like it'll be much easier to embed that knowledge in the UI script
# (rather than transmit a reasonable error back), plus it'll be more user-
# friendly, since they won't have to log in to get error about requesting
# the wrong queue length.
MINUTES=$(((${REMAINING_LIFETIME} + ${CLEAN_UP_TIME})/60))

if [[ $WHOLE_NODE ]]; then
    # Whole node jobs request the same number of tasks per node as tasks total.
    # They make no specific requests about CPUs, memory, etc., since the SLURM
    # partition should already determine that.
    SBATCH_RESOURCES_LINES="\
#SBATCH --nodes=${NODES}
#SBATCH --ntasks=${NODES}
"
else
    # Jobs on shared (non-whole-node) SLURM partitions can't be multi-node on
    # Expanse.  Request one job, and specify the resources that should be
    # allocated to the job.
    #
    # However, Delta doesn't have that restriction, so we'll have to do the
    # enforcement in the front end.
    SBATCH_RESOURCES_LINES="\
#SBATCH --ntasks=${NODES}
#SBATCH --nodes=${NODES}
"
    if [[ $CPUS ]]; then
        SBATCH_RESOURCES_LINES="\
${SBATCH_RESOURCES_LINES}
#SBATCH --cpus-per-task=${CPUS}
"
    fi
    if [[ $MEM_MB ]]; then
        SBATCH_RESOURCES_LINES="\
${SBATCH_RESOURCES_LINES}
#SBATCH --mem=${MEM_MB}M
"
    fi
fi

if [[ $GPUS ]]; then
        SBATCH_RESOURCES_LINES="\
${SBATCH_RESOURCES_LINES}
#SBATCH ${SYSTEM_SPECIFIC_GPU_FLAG}=${GPUS}
"
fi

if [[ -n $ALLOCATION ]]; then
    SBATCH_ALLOCATION_LINE="#SBATCH -A ${ALLOCATION}"
fi

# A hack to avoid adding Yet Another Command-line Argument.  It
# seemed better than either making a Perlmutter-specific pilot
# script or having the pilot script be explicitly system-aware.
#
# The regexes here are a little dodgy, and should be tweaked if
# this hack is expanded; see the PR.
if [[ $QUEUE_NAME =~ "q," ]]; then
    QUEUE_NAME=${QUEUE_NAME/*q,/}
    SBATCH_QUEUE_LINE="#SBATCH -q ${QUEUE_NAME}"
else
    SBATCH_QUEUE_LINE="#SBATCH -p ${QUEUE_NAME}"
fi

echo '#!/bin/bash' > "${PILOT_DIR}/hpc.slurm"
echo "
#SBATCH -J ${JOB_NAME}
#SBATCH -o ${PILOT_DIR}/%j.out
#SBATCH -e ${PILOT_DIR}/%j.err
${SBATCH_QUEUE_LINE}
${SBATCH_RESOURCES_LINES}
#SBATCH -t ${MINUTES}
${SBATCH_ALLOCATION_LINE}
${SBATCH_CONSTRAINT_LINE}

# This will block until all of the pilots terminate.
srun -K0 -W0 -n ${NODES} ${PILOT_BIN} ${PILOT_DIR}
rm -fr ${PILOT_DIR}

" >> "${PILOT_DIR}/hpc.slurm"


#
# Submit the SLURM job.
#
echo -e "\rStep 8 of 8: Submitting SLURM job............"
SBATCH_LOG=${PILOT_DIR}/sbatch.log
sbatch "${PILOT_DIR}/hpc.slurm" &> "${SBATCH_LOG}"
SBATCH_ERROR=$?
if [[ $SBATCH_ERROR != 0 ]]; then
    echo "Failed to submit job to SLURM (${SBATCH_ERROR}), aborting."
    cat ${SBATCH_LOG}
    exit 6
fi
JOB_ID=`cat ${SBATCH_LOG} | awk '/^Submitted batch job/{print $4}'`
echo "${CONTROL_PREFIX} JOB_ID ${JOB_ID}"
echo ""

# Reset the EXIT trap so that we don't delete the temporary directory
# that the SLURM job needs.  (We pass it the temporary directory so that
# it can clean up after itself.)
trap EXIT
exit 0
