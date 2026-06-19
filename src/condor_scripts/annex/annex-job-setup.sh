#!/bin/bash

IWD=${PWD}
ANNEX_LOGDIR=${IWD}/annex-logs.${SLURM_JOBID}

if [ -z $1 ] ; then
    echo "$(date) $(hostname) Missing PILOT_DIR"
    exit 1
fi
PILOT_DIR=$1

# If we abort early, cleanup the files we've created
function cleanup() {
    echo "$(date) $(hostname) Setup failed, cleaning up files..."
    cd ${IWD}
    rm -rf ${PILOT_DIR}
}
trap cleanup EXIT

set -o errexit

echo "$(date) $(hostname) Creating log directory ${ANNEX_LOGDIR}"
echo "$(date) $(hostname)   Logs will be available here after the Annex job completes"
mkdir -p ${ANNEX_LOGDIR}

echo "$(date) $(hostname) Creating temporary directory ${PILOT_DIR}"
mkdir -p ${PILOT_DIR}
cd ${PILOT_DIR}

echo "$(date) $(hostname) Extracting HTCondor binaries from ${IWD}/condor.tar.gz"
tar xf ${IWD}/condor.tar.gz
cd condor-*
echo "$(date) $(hostname) Configuring HTCondor EP"
bin/make-personal-from-tarball
# annex-node.sh will create the alternate local and log directories on each node
echo "LOCAL_DIR = $(pwd)/\$(FULL_HOSTNAME)
LOG = ${ANNEX_LOGDIR}/log.\$(FULL_HOSTNAME)" >> etc/condor_config
rm local/config.d/00-personal-condor
cp ${IWD}/00-annex-pilot-base local/config.d/
cp ${IWD}/20-annex-pilot-instance local/config.d/
if [ -f ~/.condor/annex_pilot_config ] ; then
    cp ~/.condor/annex_pilot_config local/config.d/90-annex-pilot-custom
fi
cp -a ${IWD}/annex.token local/tokens.d
cp -a ${IWD}/annex.password local/passwords.d

# Reset the EXIT trap so that we don't delete the setup files we've created.
trap EXIT
exit 0
