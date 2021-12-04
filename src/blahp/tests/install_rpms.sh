#!/bin/bash

set -ex

OS_VERSION=$1
BUILD_ENV=$2

# After building the RPM, try to install it
# Fix the lock file error on EL7.  /var/lock is a symlink to /var/run/lock
mkdir -p /var/run/lock

RPM_LOCATION=/tmp/rpmbuild/RPMS/x86_64
[[ $BUILD_ENV == osg ]] && extra_repos='--enablerepo=osg-upcoming-development'

package_version=`grep Version blahp/rpm/blahp.spec | awk '{print $2}'`
yum localinstall -y $RPM_LOCATION/blahp-${package_version}* $extra_repos

# Make sure OSG repos are around so we can get the OSG metapackages
# (osg-test may (?) need the OSG configuration)
[[ $BUILD_ENV == "osg" ]] || \
    yum install -y https://repo.opensciencegrid.org/osg/3.5/osg-3.5-el${OS_VERSION}-release-latest.rpm

# Install batch systems that will exercise the blahp in osg-test
yum install -y osg-ce-condor \
    munge \
    globus-proxy-utils \
    slurm \
    slurm-slurmd \
    slurm-slurmctld \
    slurm-perlapi \
    slurm-slurmdbd \
    mariadb-server \
    mariadb  \
    torque-server \
    torque-mom \
    torque-client \
    torque-scheduler
