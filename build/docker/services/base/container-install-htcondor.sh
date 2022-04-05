#!/bin/bash
set -eu
PS4='+ ${LINENO}: '

# Options
# TODO Should these be specified in the Dockerfile instead?
update_os=false
extra_packages_centos=(openssh-clients openssh-server supervisor)
extra_packages_ubuntu=(ssh supervisor)
repo_base_ubuntu=https://research.cs.wisc.edu/htcondor/repo/ubuntu
repo_base_centos=https://research.cs.wisc.edu/htcondor/repo



eval "$(grep '^\(ID\|VERSION_ID\|UBUNTU_CODENAME\)=' /etc/os-release | sed -e 's/^/OS_/')"
export OS_ID                   # ubuntu or centos
export OS_VERSION_ID           # 7, 8, or 18.04 or 20.04
export OS_UBUNTU_CODENAME      # bionic or focal (or undefined on centos)


# XXX Update me if we get more platform support
if [[ $OS_ID != centos && $OS_ID != ubuntu ]]; then
    echo "This script does not support this platform" >&2
    exit 1
fi

# XXX Remove me if we get Ubuntu dailies
if [[ $OS_ID != centos && $HTCONDOR_VERSION == daily ]]; then
    echo "HTCondor daily builds aren't available on this platform" >&2
    exit 1
fi


# Override HTCONDOR_SERIES if HTCONDOR_VERSION is fully specified
if [[ $HTCONDOR_VERSION =~ ^[0-9]+\.[0-9]+\.[0-9]+ ]]; then
    HTCONDOR_SERIES=${HTCONDOR_VERSION%.*}
    # XXX Remove once this is fixed (HTCONDOR-275)
    if [[ $OS_ID == ubuntu ]]; then
        echo "Ubuntu does not support specifying exact versions" >&2
        exit 1
    fi
fi


if ! getent passwd condor; then
    if ! getent group condor; then
        groupadd -g 64 -r condor
    fi
    useradd -r -g condor -d /var/lib/condor -s /sbin/nologin \
            -u 64 -c "Owner of HTCondor Daemons" condor
fi


# CentOS ---------------------------------------------------------------------

if [[ $OS_ID == centos ]]; then

    yum="yum -y"
    if $update_os; then
        $yum update || :
        # ^^ ignore errors b/c some packages like "filesystem" might fail to update in a container environment
    fi
    $yum install epel-release
    if [[ $OS_VERSION_ID == 7 ]]; then
        $yum install yum-plugin-priorities
    elif [[ $OS_VERSION_ID == 8 ]]; then
        $yum install dnf-plugins-core
        # enable CentOS PowerTools repo (whose name changed between CentOS releases)
        (dnf config-manager --set-enabled powertools || dnf config-manager --set-enabled PowerTools)
    fi
    $yum install "${repo_base_centos}/${HTCONDOR_SERIES}/htcondor-release-current.el${OS_VERSION_ID}.noarch.rpm"
    rpm --import /etc/pki/rpm-gpg/*
    if [[ $HTCONDOR_VERSION == daily ]]; then
        if [[ $OS_VERSION_ID == 7 ]]; then
            yum-config-manager --enable htcondor-daily
        elif [[ $OS_VERSION_ID == 8 ]]; then
            dnf config-manager --set-enabled htcondor-daily
        fi
    fi

    $yum install "${extra_packages_centos[@]}"
    rpm -q "${extra_packages_centos[@]}"
    if [[ $HTCONDOR_VERSION == latest || $HTCONDOR_VERSION == daily ]]; then
        $yum install condor
    else
        $yum install "condor = ${HTCONDOR_VERSION}"
    fi
    rpm -q condor

    yum clean all
    rm -rf /var/cache/yum/*

# Ubuntu ---------------------------------------------------------------------

elif [[ $OS_ID == ubuntu ]]; then
    apt_install="apt-get install -qy"
    export DEBIAN_FRONTEND=noninteractive

    apt-get update -q
    if $update_os; then
        apt-get upgrade -qy
    fi
    $apt_install gnupg2 wget

    set -o pipefail
    if [[ $HTCONDOR_SERIES = 8.9 ]]; then
        wget -qO - "https://research.cs.wisc.edu/htcondor/ubuntu/HTCondor-Release.gpg.key" | apt-key add -
    else
        wget -qO - "https://research.cs.wisc.edu/htcondor/repo/keys/HTCondor-${HTCONDOR_SERIES}-Key" | apt-key add -
    fi
    set +o pipefail

    echo "deb     ${repo_base_ubuntu}/${HTCONDOR_SERIES} ${OS_UBUNTU_CODENAME} main" >> /etc/apt/sources.list
    echo "deb-src ${repo_base_ubuntu}/${HTCONDOR_SERIES} ${OS_UBUNTU_CODENAME} main" >> /etc/apt/sources.list

    apt-get update -q
    $apt_install "${extra_packages_ubuntu[@]}"
    if [[ $HTCONDOR_VERSION == latest || $HTCONDOR_VERSION == daily ]]; then
        $apt_install htcondor
    else
        # Get the version-release string because unlike RHEL I can't install with just the version
        htcondor_version_release=$(apt-cache madison htcondor | grep -F "${HTCONDOR_VERSION}" | awk '/Packages/ {print $3; exit}')
        $apt_install "htcondor=$htcondor_version_release"
    fi

    rm -rf /var/lib/apt/lists/* /var/cache/apt/*

fi

# Common ---------------------------------------------------------------------

# Create passwords directories for token or pool password auth.
#
# Only root needs to know the pool password but before 8.9.12, the condor user
# also needed to access the tokens.

install -m 0700 -o root -g root -d /etc/condor/passwords.d
real_condor_version=$(condor_config_val CONDOR_VERSION)  # easier than parsing `condor_version`
if python3 -c '
import sys
version = [int(x) for x in sys.argv[1].split(".")]
sys.exit(0 if version >= [8, 9, 12] else 1)
' "$real_condor_version"; then
    install -m 0700 -o root -g root -d /etc/condor/tokens.d
else
    install -m 0700 -o condor -g condor -d /etc/condor/tokens.d
fi


# vim:et:sw=4:sts=4:ts=8
