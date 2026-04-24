#!/bin/bash

CONTROL_PREFIX="=-.-="
echo "${CONTROL_PREFIX} PID $$"

function usage() {
    echo "Usage: ${0} [record-file]"
    echo "where record-file is a file containing the annex record data."
    echo "The default filename is 'annex.record'."
}

IWD=${PWD}
if [[ -z $1 ]]; then
    RECORD_FILE=annex.record
else
    RECORD_FILE="$1"
fi
if [ ! -f $RECORD_FILE ] ; then
    echo "Can't find record file $RECORD_FILE"
    exit 1
fi
. $RECORD_FILE

if [[ -z $SYSTEM ]]; then
    usage
    exit 1
fi

if [[ -z $VERSION ]]; then
    usage
    exit 1
fi

if [[ -z $STARTD_NOCLAIM_SHUTDOWN ]]; then
    usage
    exit 1
fi

if [[ -z $JOB_NAME ]]; then
    usage
    exit 1
fi

if [[ -z $QUEUE_NAME ]]; then
    usage
    exit 1
fi

if [[ -z $COLLECTOR ]]; then
    usage
    exit 1
fi

if [[ -z $TOKEN_FILE ]]; then
    usage
    exit 1
fi

if [[ -z $LIFETIME ]]; then
    usage
    exit 1
fi

if [[ -z $PILOT_BIN ]]; then
    usage
    exit 1
fi

if [[ -z $OWNERS ]]; then
    usage
    exit 1
fi

if [[ -z $NODES ]]; then
    usage
    exit 1
fi

if [[ $ALLOCATION == "None" ]]; then
    ALLOCATION=""
fi
if [[ -z $ALLOCATION ]]; then
    echo "Will try to use the default allocation."
fi

if [[ -z $REQUEST_ID ]]; then
    usage
    exit 1
fi

if [[ -z $PASSWORD_FILE ]]; then
    usage
    exit 1
fi

if [[ -z $SCHEDD_NAME ]]; then
    usage
    exit 1
fi

if [[ $CPUS == "None" ]]; then
    CPUS=""
fi

if [[ $MEM_MB == "None" ]]; then
    MEM_MB=""
fi

if [[ $GPUS == "None" ]]; then
    GPUS=""
fi

#
# Download and configure the pilot on the head node before running it
# on the execute node(s).
#
# The following variables are constants.
#


# The binaries must be a tarball named condor-*, and unpacking that tarball
# must create a directory which also matches condor-*.
BINARIES_URL_BASE=https://htcss-downloads.chtc.wisc.edu/tarball/

SCRATCH=${IWD}
BINARIES_BASE_DIR=~/.hpc-annex/binaries

# TODO drop initial ~/.condor/annex_config and ~/.condor/annex_pilot_config
#   files if they don't already exist
if [ -f ~/.condor/annex_config ] ; then
    . ~/.condor/annex_config
fi

mkdir -p "$BINARIES_BASE_DIR"

#
# Download the binaries.
#
echo -e "\rStep 1 of 8: downloading required software..."
aversion=(${VERSION//./ })
major_ver=${aversion[0]}
minor_ver=${aversion[1]}
series="${major_ver}.${minor_ver}"
if [ $minor_ver -ne 0 ]; then
    series="${major_ver}.x"
fi

arch=$(uname -m)
distro_raw=$(. /etc/os-release; echo $ID)
distro_ver=$(. /etc/os-release; echo $VERSION_ID | cut -d. -f1)

if [[ $distro_raw =~ (rhel|centos|almalinux) ]]; then
        distro="AlmaLinux${distro_ver}"
elif [[ $distro_raw =~ opensuse-leap ]]; then
        distro="openSUSE${distro_ver}"
elif [[ $distro_raw =~ debian ]]; then
        distro="Debian${distro_ver}"
elif [[ $distro_raw =~ ubuntu ]]; then
        distro="Ubuntu${distro_ver}"
fi 

BINARIES_DIR="condor-${VERSION}-${arch}_${distro}-stripped"
BINARIES_FILE="condor-${VERSION}-${arch}_${distro}-stripped.tar.gz"
RELEASE_URL="${BINARIES_URL_BASE}${series}/${VERSION}/release/${BINARIES_FILE}"
BETA_URL="${BINARIES_URL_BASE}${series}/${VERSION}/beta/${BINARIES_FILE}"
ALPHA_URL="${BINARIES_URL_BASE}${series}/${VERSION}/alpha/${BINARIES_FILE}"
SNAPSHOT_URL="${BINARIES_URL_BASE}${series}/${VERSION}/snapshot/${BINARIES_FILE}"

if [ -d ${BINARIES_BASE_DIR}/${BINARIES_DIR} ] ; then
    echo "Binaries already installed"
else
    rc=1
    if [ $rc -ne 0 ] ; then
        curl -fsSL ${RELEASE_URL} -o ${BINARIES_FILE}
        rc=$?
        if [ $rc -ne 0 ] ; then
            echo "Failed to download release build."
        fi
    fi
    if [ $rc -ne 0 ] ; then
        curl -fsSL ${BETA_URL} -o ${BINARIES_FILE}
        rc=$?
        if [ $rc -ne 0 ] ; then
            echo "Failed to download beta build."
        fi
    fi
    if [ $rc -ne 0 ] ; then
        curl -fsSL ${ALPHA_URL} -o ${BINARIES_FILE}
        rc=$?
        if [ $rc -ne 0 ] ; then
            echo "Failed to download alpha build."
        fi
    fi
    if [ $rc -ne 0 ] ; then
        curl -fsSL ${SNAPSHOT_URL} -o ${BINARIES_FILE}
        rc=$?
        if [ $rc -ne 0 ] ; then
            echo "Failed to download snapshot build."
        fi
    fi
    #CURL_LOGGING=`curl -fsSL ${RELEASE_URL} -o ${BINARIES_FILE} 2>&1`
    if [[ $? != 0 ]]; then
        echo "Failed to download binaries from '${RELEASE_URL}', aborting."
        #echo ${CURL_LOGGING}
        exit 2
    fi

    #
    # Unpack the binaries.
    #
    echo -e "\rStep 4 of 8: unpacking software.............."
    BINARIES_TMP_DIR=${BINARIES_BASE_DIR}/tmp.$$
    mkdir -p $BINARIES_TMP_DIR
    cd ${BINARIES_TMP_DIR}
    TAR_LOGGING=`tar -z -x -f ${IWD}/${BINARIES_FILE} 2>&1`
    rc=$?
    rm ${IWD}/${BINARIES_FILE}
    if [[ $rc == 0 ]]; then
	mv condor-* ${BINARIES_BASE_DIR}/${BINARIES_DIR}
    fi
    cd ${IWD}
    rm -rf ${BINARIES_TMP_DIR}
    if [[ $rc != 0 ]]; then
        echo "Failed to unpack binaries from '${BINARIES_FILE}', aborting."
        echo ${TAR_LOGGING}
        exit 3
    fi
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


echo -e "\rStep 2 of 8: Creating temporary directory...."
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

# The pilot scripts need to live in the ${PILOT_DIR} because the front-end
# copied them into a temporary directory that it's responsible for cleaning up.
mv ${IWD}/${PILOT_BIN} ${PILOT_DIR}
PILOT_BIN=${PILOT_DIR}/${PILOT_BIN}


echo -e "\rStep 3 of 8: downloading configuration......."
# This step was removed


#
# Make the personal condor.
#
#rm condor-*.tar.gz
#cd condor-*

echo -e "\rStep 5 of 8: configuring software (part 1)..."
cd ${BINARIES_BASE_DIR}/${BINARIES_DIR}
MPC_LOGGING=`./bin/make-personal-from-tarball 2>&1`
if [[ $? != 0 ]]; then
    echo "Failed to make personal condor, aborting."
    echo ${MPC_LOGGING}
    exit 4
fi
# Relocate created files to PILOT_DIR
# TODO Add external local directory mode to make-personal-from-tarball
cp -a local $PILOT_DIR 
sed -e "s|^CONDOR_CONFIG=.*|CONDOR_CONFIG=${PILOT_DIR}/local/condor_config|" condor.sh >${PILOT_DIR}/local/condor.sh
sed -e "s|^LOCAL_DIR = .*|LOCAL_DIR = ${PILOT_DIR}/local|" etc/condor_config >${PILOT_DIR}/local/condor_config
rm -rf local etc/condor_config condor.sh
cd $PILOT_DIR

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

echo -e "\rStep 6 of 8: configuring software (part 2)..."
rm local/config.d/00-personal-condor
# TODO if file not in launch-time cwd, copy from tarball
cp ${IWD}/00-annex-pilot-base local/config.d/
echo "
# These are instance-specific settings for an HPC annex EP
# The ANNEX_PILOT_* parameters are referenced by the 00-annex-pilot-base
# configuration file.

ANNEX_PILOT_COLLECTOR = ${COLLECTOR}
ANNEX_PILOT_LIFETIME = ${LIFETIME}
ANNEX_PILOT_OWNERS = ${OWNERS}
ANNEX_PILOT_JOB_NAME = ${JOB_NAME}
ANNEX_PILOT_REQUEST_ID = ${REQUEST_ID}
ANNEX_PILOT_SCHEDD_NAME = ${SCHEDD_NAME}

STARTD_NOCLAIM_SHUTDOWN = ${STARTD_NOCLAIM_SHUTDOWN}

${CONDOR_CPUS_LINE}
${CONDOR_MEMORY_LINE}
${CONDOR_GPUS_LINE}

" > local/config.d/10-annex-pilot-instance

if [ -f ~/.condor/annex_pilot_config ] ; then
    cp ~/.condor/annex_pilot_config local/config.d/90-annex-pilot-custom
fi

mkdir -p local/passwords.d
mkdir -p local/tokens.d
mv ${IWD}/${TOKEN_FILE} local/tokens.d
# On Delta, if this pair is just `mv`, instead, there's a weird warning.
cp ${IWD}/${PASSWORD_FILE} local/passwords.d/POOL
rm -f ${PASSWORD_FILE}


#
# Unpack the configuration on top.
#

echo -e "\rStep 7 of 8: configuring software (part 3)..."
# This step was removed


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
MINUTES=$((${LIFETIME}/60))

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
