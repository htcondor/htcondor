#!/bin/bash

VERSION_CODENAME='unknown'
. /etc/os-release
VERSION_CODENAME=${VERSION_CODENAME^^?}

echo "Preparing build files for ${VERSION_CODENAME}"

gpp -D${VERSION_CODENAME} control.in > control
gpp -D${VERSION_CODENAME} htcondor.install.in > htcondor.install
gpp -D${VERSION_CODENAME} rules.in > rules
chmod 755 rules
