#!/bin/bash
# Copied from https://github.com/htcondor/htcondor-ce

set -exu

# Build source and binary RPMs.
# SRPMs will be /tmp/rpmbuild/SRPMS/*.rpm.
# Binary RPMs will be /tmp/rpmbuild/RPMS/*/*.rpm.

OS_VERSION=$1
BUILD_ENV=$2

# Create the condor user/group for subsequent chowns,
# using the official RHEL UID/GID
groupadd -g 64 -r condor
useradd -r -g condor -d /var/lib/condor -s /sbin/nologin \
        -u 64 -c "Owner of HTCondor Daemons" condor

if  [[ $OS_VERSION == 7 ]]; then
    YUM_PKG_NAME="yum-plugin-priorities"
else
    YUM_PKG_NAME="yum-utils"
fi

# Clean the yum cache
yum clean all

# Exponential backoff retry for missing DNF lock file
# FileNotFoundError: [Errno 2] No such file or directory: '/var/cache/dnf/metadata_lock.pid'
# 2020-03-17T13:43:57Z CRITICAL [Errno 2] No such file or directory: '/var/cache/dnf/metadata_lock.pid'
retry=0
until yum -y -d0 update || ((++retry > 3)); do
    sleep $((5**$retry))
done
# maybe die if ((retry > 3))

yum install -y epel-release $YUM_PKG_NAME

# Broken mirror?
echo "exclude=mirror.beyondhosting.net" >> /etc/yum/pluginconf.d/fastestmirror.conf

if [[ $OS_VERSION != 7 ]]; then
    yum-config-manager --enable powertools
fi


if [[ $BUILD_ENV == osg ]]; then
    yum install -y https://repo.opensciencegrid.org/osg/3.5/osg-3.5-el${OS_VERSION}-release-latest.rpm
    yum-config-manager --enable osg-upcoming-development  # make sure to grab 8.9.13
else
    yum install -y https://research.cs.wisc.edu/htcondor/repo/current/htcondor-release-current.el${OS_VERSION}.noarch.rpm
fi

# Install packages required for the build
yum -y install \
    rpm-build \
    git \
    tar \
    gzip \
    libtool \
    gcc-c++ \
    make \
    autoconf \
    automake \
    condor-classads-devel \
    globus-gss-assist-devel \
    globus-gsi-credential-devel \
    globus-gsi-proxy-core-devel \
    globus-gsi-cert-utils-devel \
    docbook-style-xsl \
    libxslt \
    python3 \
    python3-rpm

if [[ $BUILD_ENV == uw_build ]]; then
    printf "%s\n" "%dist .el${OS_VERSION}" >> /etc/rpm/macros.dist
else
    printf "%s\n" "%dist .${BUILD_ENV}.el${OS_VERSION}" >> /etc/rpm/macros.dist
fi
printf "%s\n" "%${BUILD_ENV} 1" >> /etc/rpm/macros.dist

mkdir -p /tmp/rpmbuild/{SOURCES,SPECS}
cp blahp/rpm/blahp.spec /tmp/rpmbuild/SPECS
package_version=`grep Version blahp/rpm/blahp.spec | awk '{print $2}'`
pushd blahp
git archive --format=tar --prefix=blahp-${package_version}/ HEAD | \
    gzip > /tmp/rpmbuild/SOURCES/blahp-${package_version}.tar.gz
popd

# Build the SRPM; don't put a dist tag in it
rpmbuild --define '_topdir /tmp/rpmbuild' --undefine 'dist' -bs /tmp/rpmbuild/SPECS/blahp.spec
# Build the binary RPM
rpmbuild --define '_topdir /tmp/rpmbuild' -bb /tmp/rpmbuild/SPECS/blahp.spec

# dir needs to be inside htcondor-ce so it's visible outside the container
mkdir -p blahp/ci_deploy
cp -f /tmp/rpmbuild/RPMS/*/*.rpm blahp/ci_deploy/
cp -f /tmp/rpmbuild/SRPMS/*.rpm blahp/ci_deploy/
