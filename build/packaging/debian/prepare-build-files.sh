#!/bin/bash

VERSION_CODENAME='unknown'
. /etc/os-release
VERSION_CODENAME=${VERSION_CODENAME^^?}

if [ $VERSION_CODENAME = 'TRIXIE' ]; then
    VERSION_CODENAME='SID'
fi

ARCH=$(arch)
ARCH=${ARCH^^?}

echo "Preparing build files for ${VERSION_CODENAME} on ${ARCH}"

gpp -D${VERSION_CODENAME} -D"${ARCH}" condor-test.install.in > condor-test.install
gpp -D${VERSION_CODENAME} -D"${ARCH}" control.in > control
gpp -D${VERSION_CODENAME} -D"${ARCH}" copyright.in > copyright
gpp -D${VERSION_CODENAME} -D"${ARCH}" rules.in > rules
chmod 755 rules

if [ $VERSION_CODENAME = 'SID' ]; then
    rm condor-tarball.install
fi
