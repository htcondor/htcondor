#!/bin/bash
set -eu
PS4='+ ${LINENO}: '

# Options
# TODO Should these be specified in the Dockerfile instead?
update_os=false
extra_packages_centos=(openssh-clients openssh-server supervisor)
extra_packages_ubuntu=(ssh supervisor)
# add some basic debugging tools too
extra_packages_centos+=(procps-ng less nano)
extra_packages_ubuntu+=(procps less nano)
repo_base_ubuntu=https://research.cs.wisc.edu/htcondor/repo/ubuntu
repo_base_centos=https://research.cs.wisc.edu/htcondor/repo



eval "$(grep '^\(ID\|VERSION_ID\|UBUNTU_CODENAME\)=' /etc/os-release | sed -e 's/^/OS_/')"
# Alma Linux has a version like 8.6 (We use the Ubuntu Codename, not version)
OS_VERSION_ID=${OS_VERSION_ID%%.*}
export OS_ID                   # ubuntu or centos or almalinux
export OS_VERSION_ID           # 7, 8, or 18.04 or 20.04
export OS_UBUNTU_CODENAME      # bionic or focal (or undefined on centos)


# XXX Update me if we get more platform support
if [[ $OS_ID != centos && $OS_ID != almalinux && $OS_ID != ubuntu ]]; then
    echo "This script does not support this platform" >&2
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


if [[ $HTCONDOR_SERIES =~ ^([0-9]+)\.([0-9]+) ]]; then
    # turn a post-9.0 feature series like 9.5 into 9.x
    series_major=${BASH_REMATCH[1]}
    series_minor=${BASH_REMATCH[2]}

    if [[ $series_minor -gt 0 ]]; then
        series_str=${series_major}.x
    else
        series_str=${series_major}.0
    fi
elif [[ $HTCONDOR_SERIES =~ ^([0-9]+)\.x ]]; then
    series_str=$HTCONDOR_SERIES
    series_minor=1
else
    echo "Could not parse \$HTCONDOR_SERIES $HTCONDOR_SERIES" >&2
    exit 2
fi



if ! getent passwd condor; then
    if ! getent group condor; then
        groupadd -g 64 -r condor
    fi
    useradd -r -g condor -d /var/lib/condor -s /sbin/nologin \
            -u 64 -c "Owner of HTCondor Daemons" condor
fi


# CentOS ---------------------------------------------------------------------

centos_enable_repo () {
    if [[ $OS_VERSION_ID -lt 8 ]]; then
        yum-config-manager --enable "$1"
    else
        dnf config-manager --set-enabled "$1"
    fi
}


if [[ $OS_ID == centos || $OS_ID == almalinux ]]; then

    yum="yum -y"
    if $update_os; then
        $yum update || :
        # ^^ ignore errors b/c some packages like "filesystem" might fail to update in a container environment
    fi
    $yum install epel-release
    if [[ $OS_VERSION_ID -lt 8 ]]; then
        $yum install yum-plugin-priorities
    else
        $yum install dnf-plugins-core
    fi
    if [[ $OS_VERSION_ID -eq 8 ]]; then
        centos_enable_repo powertools
    fi
    $yum install "${repo_base_centos}/${series_str}/htcondor-release-current.el${OS_VERSION_ID}.noarch.rpm"
    rpm --import /etc/pki/rpm-gpg/*
    # Update repository is only available for feature versions
    if [[ $series_minor -gt 0 ]]; then
        centos_enable_repo htcondor-update
    fi
    if [[ $HTCONDOR_VERSION == daily ]]; then
        centos_enable_repo htcondor-daily
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
    wget -qO - "https://research.cs.wisc.edu/htcondor/repo/keys/HTCondor-${series_str}-Key" | apt-key add -
    set +o pipefail

    echo "deb     ${repo_base_ubuntu}/${series_str} ${OS_UBUNTU_CODENAME} main" >> /etc/apt/sources.list
    echo "deb-src ${repo_base_ubuntu}/${series_str} ${OS_UBUNTU_CODENAME} main" >> /etc/apt/sources.list
    # Update repository is only available for feature versions
    if [[ $series_minor -gt 0 ]]; then
        echo "deb     ${repo_base_ubuntu}/${series_str}-update ${OS_UBUNTU_CODENAME} main" >> /etc/apt/sources.list
        echo "deb-src ${repo_base_ubuntu}/${series_str}-update ${OS_UBUNTU_CODENAME} main" >> /etc/apt/sources.list
    fi
    if [[ $HTCONDOR_VERSION == daily ]]; then
        set -o pipefail
        wget -qO - "https://research.cs.wisc.edu/htcondor/repo/keys/HTCondor-${series_str}-Daily-Key" | apt-key add -
        set +o pipefail
        echo "deb     ${repo_base_ubuntu}/${series_str}-daily ${OS_UBUNTU_CODENAME} main" >> /etc/apt/sources.list
        echo "deb-src ${repo_base_ubuntu}/${series_str}-daily ${OS_UBUNTU_CODENAME} main" >> /etc/apt/sources.list
    fi

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

install -m 0700 -o root -g root -d /etc/condor/passwords.d
install -m 0700 -o root -g root -d /etc/condor/tokens.d


# vim:et:sw=4:sts=4:ts=8
