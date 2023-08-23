#!/bin/bash

VERSION_CODENAME='unknown'
. /etc/os-release
VERSION_CODENAME=${VERSION_CODENAME^^?}

ARCH=$(arch)
ARCH=${ARCH^^?}

echo "Preparing build files for ${VERSION_CODENAME} on ${ARCH}"

gpp -D${VERSION_CODENAME} -D${ARCH} control.in > control
chmod 755 rules
