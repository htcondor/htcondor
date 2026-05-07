#!/bin/bash

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

if [[ -z $TOKEN_FILE ]]; then
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

if [[ -z $PASSWORD_FILE ]]; then
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

BINARIES_FILE="condor-${VERSION}-${arch}_${distro}-stripped.tar.gz"
RELEASE_URL="${BINARIES_URL_BASE}${series}/${VERSION}/release/${BINARIES_FILE}"
BETA_URL="${BINARIES_URL_BASE}${series}/${VERSION}/beta/${BINARIES_FILE}"
ALPHA_URL="${BINARIES_URL_BASE}${series}/${VERSION}/alpha/${BINARIES_FILE}"
SNAPSHOT_URL="${BINARIES_URL_BASE}${series}/${VERSION}/snapshot/${BINARIES_FILE}"

if [ -f ${BINARIES_DIR}/${BINARIES_FILE} ] ; then
    echo "Binaries already installed"
else
    rc=1
    if [ $rc -ne 0 ] ; then
        curl -fsSL ${RELEASE_URL} -o ${BINARIES_DIR}/${BINARIES_FILE}
        rc=$?
        if [ $rc -ne 0 ] ; then
            echo "Failed to download release build."
        fi
    fi
    if [ $rc -ne 0 ] ; then
        curl -fsSL ${BETA_URL} -o ${BINARIES_DIR}/${BINARIES_FILE}
        rc=$?
        if [ $rc -ne 0 ] ; then
            echo "Failed to download beta build."
        fi
    fi
    if [ $rc -ne 0 ] ; then
        curl -fsSL ${ALPHA_URL} -o ${BINARIES_DIR}/${BINARIES_FILE}
        rc=$?
        if [ $rc -ne 0 ] ; then
            echo "Failed to download alpha build."
        fi
    fi
    if [ $rc -ne 0 ] ; then
        curl -fsSL ${SNAPSHOT_URL} -o ${BINARIES_DIR}/${BINARIES_FILE}
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

fi

ln -s ${BINARIES_DIR}/${BINARIES_FILE} ${IWD}/condor.tar.gz

mv ${TOKEN_FILE} annex.token
mv ${PASSWORD_FILE} annex.password

echo -e "\rStep 3 of 8: downloading configuration......."
# This step was removed


echo -e "\rStep 6 of 8: configuring software (part 2)..."
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

" > ${IWD}/10-annex-pilot-instance


#
# Unpack the configuration on top.
#

echo -e "\rStep 7 of 8: configuring software (part 3)..."
# This step was removed


#
# Write the SLURM job.
#

echo '#!/bin/bash' > "${IWD}/hpc.slurm"
echo "
#SBATCH -J ${JOB_NAME}
#SBATCH -o %j.out
#SBATCH -e %j.err" >>${IWD}/hpc.slurm

if [ -f ~/.condor/annex_slurm_args ] ; then
    cat ~/.condor/annex_slurm_args >>${IWD}/hpc.slurm
fi

echo "
SCRATCH=\${PWD}
if [ -f ~/.condor/annex_config ] ; then
    . ~/.condor/annex_config
fi
PILOT_DIR=\${SCRATCH}/pilot.\${SLURM_JOBID}

./annex-job-setup.sh \$PILOT_DIR

# This will block until all of the pilots terminate.
srun -K0 -W0 annex-node.sh \${PILOT_DIR}
rm -fr \${PILOT_DIR}
" >>${IWD}/hpc.slurm


echo "Setup is complete.
The SLURM job script is hpc.slurm.
Please edit the #SBATCH options as necessary, then submit with sbatch.
"

# Reset the EXIT trap so that we don't delete the temporary directory
# that the SLURM job needs.  (We pass it the temporary directory so that
# it can clean up after itself.)
trap EXIT
exit 0
