#!/bin/bash

IWD=${PWD}

function usage() {
    echo "Usage: ${0} [record-file]"
    echo "where record-file is a file containing the annex record data."
    echo "The default filename is 'annex.record'."
}

# If we abort early, cleanup the files we've created
function cleanup() {
    echo "Setup failed, cleaning up files..."
    rm -f ${IWD}/condor.tar.gz ${IWD}/hpc.slurm ${IWD}/20-annex-pilot-instance ${CLEANUP_TARBALL}
}
trap cleanup EXIT

set -o errexit

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

if [[ -z $COLLECTOR ]]; then
    usage
    exit 1
fi

if [[ -z $OWNERS ]]; then
    usage
    exit 1
fi

if [[ -z $REQUEST_ID ]]; then
    usage
    exit 1
fi

if [[ -z $SCHEDD_NAME ]]; then
    usage
    exit 1
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
BINARIES_DIR=~/.hpc-annex/binaries

# TODO drop initial ~/.condor/annex_config and ~/.condor/annex_pilot_config
#   files if they don't already exist
if [ -f ~/.condor/annex_config ] ; then
    . ~/.condor/annex_config
fi

mkdir -p "$BINARIES_DIR"

#
# Download the binaries.
#
echo -e "\rStep 1 of 3: Downloading HTCondor ${VERSION}..."
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

if [[ $distro_raw =~ (rhel|centos|almalinux|rocky) ]]; then
    distro="AlmaLinux${distro_ver}"
elif [[ $distro_raw =~ opensuse-leap ]]; then
    distro="openSUSE${distro_ver}"
elif [[ $distro_raw =~ debian ]]; then
    distro="Debian${distro_ver}"
elif [[ $distro_raw =~ ubuntu ]]; then
    distro="Ubuntu${distro_ver}"
else
    echo "Unrecognized linux distro '${distro_raw}', aborting setup."
    exit 1
fi

BINARIES_FILE="condor-${VERSION}-${arch}_${distro}-stripped.tar.gz"
RELEASE_URL="${BINARIES_URL_BASE}${series}/${VERSION}/release/${BINARIES_FILE}"
BETA_URL="${BINARIES_URL_BASE}${series}/${VERSION}/beta/${BINARIES_FILE}"
ALPHA_URL="${BINARIES_URL_BASE}${series}/${VERSION}/alpha/${BINARIES_FILE}"
SNAPSHOT_URL="${BINARIES_URL_BASE}${series}/${VERSION}/snapshot/${BINARIES_FILE}"

# Some of these downloads may fail, which is not fatal.
# We'll exit explicitly on a fatal failure.
set +o errexit

if [ -f ${BINARIES_DIR}/${BINARIES_FILE} ] ; then
    echo "HTCondor tarball is already downloaded"
else
    CLEANUP_TARBALL=${BINARIES_DIR}/${BINARIES_FILE}
    rc=1
    if [ $rc -ne 0 ] ; then
        CURL_LOGGING_1=`curl -fsSL ${RELEASE_URL} -o ${BINARIES_DIR}/${BINARIES_FILE} 2>&1`
        rc=$?
        if [ $rc -ne 0 ] ; then
            echo "Failed to download release build, will try pre-release builds"
        fi
    fi
    if [ $rc -ne 0 ] ; then
        CURL_LOGGING_2=`curl -fsSL ${BETA_URL} -o ${BINARIES_DIR}/${BINARIES_FILE} 2>&1`
        rc=$?
    fi
    if [ $rc -ne 0 ] ; then
        CURL_LOGGING_3=`curl -fsSL ${ALPHA_URL} -o ${BINARIES_DIR}/${BINARIES_FILE} 2>&1`
        rc=$?
    fi
    if [ $rc -ne 0 ] ; then
        CURL_LOGGING_4=`curl -fsSL ${SNAPSHOT_URL} -o ${BINARIES_DIR}/${BINARIES_FILE} 2>&1`
        rc=$?
    fi
    if [[ $rc != 0 ]]; then
        echo "Failed to download binaries from '${RELEASE_URL}', aborting."
        echo ${CURL_LOGGING_1}
        exit 2
    fi
    echo "HTCondor tarball has been downloaded to ${BINARIES_DIR}/${BINARIES_FILE}"
    CLEANUP_TARBALL=""
fi

# Re-enable errexit
set -o errexit

ln -s ${BINARIES_DIR}/${BINARIES_FILE} ${IWD}/condor.tar.gz

echo -e "\rStep 2 of 3: Cleaning old logs..."
find . -maxdepth 1 -name 'annex-logs.*' -mtime +14 -exec rm -rf '{}' ';'


echo -e "\rStep 3 of 3: configuring software..."

cat << EOF > ${IWD}/20-annex-pilot-instance
# These are instance-specific settings for an HPC annex EP
# The ANNEX_PILOT_* parameters are referenced by the 00-annex-pilot-base
# configuration file.

ANNEX_PILOT_COLLECTOR = ${COLLECTOR}
ANNEX_PILOT_OWNERS = ${OWNERS}
ANNEX_PILOT_JOB_NAME = ${JOB_NAME}
ANNEX_PILOT_REQUEST_ID = ${REQUEST_ID}
ANNEX_PILOT_SCHEDD_NAME = ${SCHEDD_NAME}

STARTD_NOCLAIM_SHUTDOWN = ${STARTD_NOCLAIM_SHUTDOWN}

EOF

#
# Write the SLURM job script.
#

cat << EOF > ${IWD}/hpc.slurm
#!/bin/bash

#SBATCH -J ${JOB_NAME}
#SBATCH -o %j.out
#SBATCH -e %j.err
EOF

if [ -f ~/.condor/annex_slurm_args ] ; then
    cat ~/.condor/annex_slurm_args >>${IWD}/hpc.slurm
fi

# Don't do shell macro expansion here
cat << 'EOF' >> ${IWD}/hpc.slurm

echo "$(date) $(hostname) Annex job starting"
export ANNEX_JOBID=${SLURM_JOBID:-pid-$$}
SCRATCH=${PWD}
if [ -f ~/.condor/annex_config ] ; then
    . ~/.condor/annex_config
fi
PILOT_DIR=${SCRATCH}/pilot.${ANNEX_JOBID}

echo "$(date) $(hostname) Performing setup tasks"
./annex-job-setup.sh $PILOT_DIR

# This will block until all of the pilots terminate.
echo "$(date) $(hostname) Launching EPs"
if [ -z "$SLURM_JOBID" ] ; then
    ./annex-node.sh ${PILOT_DIR}
else
    srun -K0 -W0 annex-node.sh ${PILOT_DIR}
fi
echo "$(date) $(hostname) All EPs have exited, performing cleanup"
echo "$(date) $(hostname) Removing temporary directory ${PILOT_DIR}"
rm -fr ${PILOT_DIR}
echo "$(date) $(hostname) Annex job complete, exiting"
EOF


echo "Setup is complete.
The SLURM job script is hpc.slurm.
Please edit the #SBATCH options as necessary, then submit with sbatch.
"

# Reset the EXIT trap so that we don't delete the setup files we've created.
trap EXIT
exit 0
