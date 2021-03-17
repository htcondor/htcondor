#!/bin/bash
set -eu
PS4='+ ${LINENO}: '

eval "$(grep '^\(ID\|VERSION_ID\)=' /etc/os-release | sed -e 's/^/OS_/')"
export OS_ID                   # ubuntu or centos
export OS_VERSION_ID           # 7, 8, or 18.04 or 20.04


# XXX Update me if we get more platform support
if [[ $OS_ID != centos && $OS_ID != ubuntu ]]; then
    echo "This script does not support this platform" >&2
    exit 1
fi

PACKAGE_LIST_TXT=${1-}

if [[ $OS_ID == centos ]]; then
    if [[ $OS_VERSION_ID == 7 ]]; then
        yum-config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo
    else
        dnf config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo
    fi
    yum install -y docker-ce-cli
    rpm -q docker-ce-cli

    if [[ -f $PACKAGE_LIST_TXT ]]; then
        grep '^[A-Za-z0-9]' "$PACKAGE_LIST_TXT" | \
            (xargs -r yum -y install && \
            xargs -r rpm -q)
    fi
    yum clean all
    rm -rf /var/cache/yum/*
elif [[ $OS_ID == ubuntu ]]; then
    if [[ -f $PACKAGE_LIST_TXT ]]; then
        export DEBIAN_FRONTEND=noninteractive
        apt-get update -q
        grep '^[A-Za-z0-9]' "$PACKAGE_LIST_TXT" | \
            xargs -r apt-get install -qy
        rm -rf /var/lib/apt/lists/* /var/cache/apt/*
    fi
fi

# vim:et:sw=4:sts=4:ts=8
