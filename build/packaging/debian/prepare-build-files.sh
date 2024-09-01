#!/bin/bash

VERSION_CODENAME='unknown'
. /etc/os-release
VERSION_CODENAME=${VERSION_CODENAME^^?}

ARCH=$(arch)
ARCH=${ARCH^^?}

UW_BUILD="$1"

echo "Preparing build files for ${VERSION_CODENAME} on ${ARCH}"

gpp ${UW_BUILD} -D${VERSION_CODENAME} -D"${ARCH}" condor-annex-ec2.install.in > condor-annex-ec2.install
gpp ${UW_BUILD} -D${VERSION_CODENAME} -D"${ARCH}" condor.install.in > condor.install
gpp ${UW_BUILD} -D${VERSION_CODENAME} -D"${ARCH}" condor-test.install.in > condor-test.install
gpp ${UW_BUILD} -D${VERSION_CODENAME} -D"${ARCH}" control.in > control
gpp ${UW_BUILD} -D${VERSION_CODENAME} -D"${ARCH}" copyright.in > copyright
gpp ${UW_BUILD} -D${VERSION_CODENAME} -D"${ARCH}" rules.in > rules
chmod 755 rules

if [ "${UW_BUILD}" != '-DUW_BUILD' ]; then
    rm condor-tarball.install
fi
