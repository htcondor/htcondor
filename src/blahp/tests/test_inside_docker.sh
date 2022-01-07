#!/bin/sh

set -xe

OS_VERSION=$1
BUILD_ENV=$2

# Pre-create vdttest user so that we can create the BLAHP status cache
# debugging logs
useradd -m vdttest

# Source repo version
git clone https://github.com/opensciencegrid/osg-test.git
pushd osg-test
git rev-parse HEAD
make install
popd
git clone https://github.com/opensciencegrid/osg-ca-generator.git
pushd osg-ca-generator
git rev-parse HEAD
make install
popd

# Bind on the right interface and skip hostname checks.
cat << "EOF" > /etc/condor/config.d/99-local.conf
BIND_ALL_INTERFACES = true
GSI_SKIP_HOST_CHECK=true
ALL_DEBUG=$(ALL_DEBUG) D_FULLDEBUG D_CAT
SCHEDD_INTERVAL=1
SCHEDD_MIN_INTERVAL=1
JOB_ROUTER_POLLING_PERIOD=1
GRIDMANAGER_JOB_PROBE_INTERVAL=1
EOF
cp /etc/condor/config.d/99-local.conf /etc/condor-ce/config.d/99-local.conf

# Reduce the trace timeouts
export _condor_CONDOR_CE_TRACE_ATTEMPTS=120

# Enable PBS/Slurm BLAH debugging
mkdir /var/tmp/{qstat,slurm}_cache_vdttest/
touch /var/tmp/qstat_cache_vdttest/pbs_status.debug
touch /var/tmp/slurm_cache_vdttest/slurm_status.debug
chown -R vdttest: /var/tmp/*_cache_vdttest/

# Ok, do actual testing
echo "------------ OSG Test --------------"
osg-test -mvd --hostcert --no-cleanup
