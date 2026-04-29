#!/bin/bash

IWD=${PWD}

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
rm local/config.d/00-personal-condor
cp ${IWD}/00-annex-pilot-base local/config.d/
cp ${IWD}/10-annex-pilot-instance local/config.d/
if [ -f ~/.condor/annex_pilot_config ] ; then
    cp ~/.condor/annex_pilot_config local/config.d/90-annex-pilot-custom
fi
cp -a ${IWD}/annex.token local/tokens.d
cp -a ${IWD}/annex.password local/passwords.d
