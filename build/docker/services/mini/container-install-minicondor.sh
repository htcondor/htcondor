#!/bin/bash
set -eu
PS4='+ ${LINENO}: '

eval "$(grep '^\(ID\|VERSION_ID\)=' /etc/os-release | sed -e 's/^/OS_/')"
# Alma Linux has a version like 8.6 (We use the Ubuntu Codename, not version)
OS_VERSION_ID=${OS_VERSION_ID%%.*}
export OS_ID                   # ubuntu or centos or almalinux
export OS_VERSION_ID           # 7, 8, or 18.04 or 20.04


# XXX Update me if we get more platform support
if [[ $OS_ID != centos && $OS_ID != almalinux && $OS_ID != ubuntu ]]; then
    echo "This script does not support this platform" >&2
    exit 1
fi

getent passwd restd || useradd -m restd
getent passwd submituser || useradd -m submituser

if [[ $OS_ID == centos || $OS_ID == almalinux ]]; then

    # HTCONDOR-236
    rm -f /tmp/00-minicondor-ubuntu

    # perl is needed for condor_run. Reinstalling condor to get the man pages too.
    # git is needed for the restd; jq is too useful not to include.
    yum='yum -y'
    $yum install perl
    rpm -q perl
    $yum install --setopt=tsflags='' man-db
    rpm -q man-db
    $yum reinstall --setopt=tsflags='' "*condor*"

    HTCONDOR_VERSION=$(rpm -q --qf='%{VERSION}\n' condor)
    $yum install --setopt=tsflags='' "minicondor = ${HTCONDOR_VERSION}"
    rpm -q minicondor
    $yum install -y jq git-core
    rpm -q jq
    (rpm -q git || rpm -q git-core)

    yum clean all
    rm -rf /var/cache/yum/*

    pip3=/usr/bin/pip-3

elif [[ $OS_ID == ubuntu ]]; then

    # HTCONDOR-236 -- can't install minihtcondor in a container because
    # it tries to use systemd.  Add the config file manually.
    mv -f /tmp/00-minicondor-ubuntu /etc/condor/00-minicondor

    export DEBIAN_FRONTEND=noninteractive
    apt-get update -q
    apt-get install -qy git jq perl python3-pip
    rm -rf /var/lib/apt/lists/* /var/cache/apt/*

    pip3=/usr/bin/pip3

fi

$pip3 install flask_restful

# vim:et:sw=4:sts=4:ts=8
