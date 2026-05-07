#!/bin/bash

IWD=${PWD}
ANNEX_LOGDIR=${IWD}/annex-logs.${SLURM_JOBID}

mkdir ${ANNEX_LOGDIR}

if [ -z $1 ] ; then
    echo "Missing PILOT_DIR"
    exit 1
fi
PILOT_DIR=$1

mkdir -p ${PILOT_DIR}
cd ${PILOT_DIR}

tar xvf ${IWD}/condor.tar.gz
cd condor-*
bin/make-personal-from-tarball
# annex-node.sh will create the alternate local and log directories on each node
echo "LOCAL_DIR = $(pwd)/\$(FULL_HOSTNAME)
LOG = ${ANNEX_LOGDIR}/log.\$(FULL_HOSTNAME)" >> etc/condor_config
rm local/config.d/00-personal-condor
cp ${IWD}/00-annex-pilot-base local/config.d/
cp ${IWD}/10-annex-pilot-instance local/config.d/
if [ -f ~/.condor/annex_pilot_config ] ; then
    cp ~/.condor/annex_pilot_config local/config.d/90-annex-pilot-custom
fi
cp -a ${IWD}/annex.token local/tokens.d
cp -a ${IWD}/annex.password local/passwords.d
