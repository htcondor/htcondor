#!/bin/bash
if grep -qi bullseye /etc/os-release; then
    codename='BULLSEYE'
elif grep -qi bookworm /etc/os-release; then
    codename='BOOKWORM'
elif grep -qi focal /etc/os-release; then
    codename='FOCAL'
elif grep -qi jammy /etc/os-release; then
    codename='JAMMY'
else
    echo "Unknown Debian/Ubuntu distribution"
    exit 1
fi

echo "Preparing build files for ${codename}"

gpp -D${codename} control.in > control
# Nolonger needed: gpp -D${codename} htcondor.install.in > htcondor.install
# Nolonger needed: gpp -D${codename} rules.in > rules
# Nolonger needed: chmod 755 rules
