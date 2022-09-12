#!/bin/bash
# Installs files needed for a pilot container of htcondor on EL 8
# These consist of the condor RPM and its dependencies, and
# singularity and its dependencies.

prog=${0##*/}


fail () {
    set +x
    ret=$1; shift
    echo "$prog:" "$@" >&2
    exit $ret
}


set -o nounset

export PS4='+ ${LINENO}: '
set -x

yum install -y \
    yum-utils \
    epel-release \
    https://research.cs.wisc.edu/htcondor/repo/9.x/htcondor-release-current.el8.noarch.rpm \
    || fail 65 "YUM repo installation failed"
yum-config-manager --set-enabled powertools
yum-config-manager --setopt install_weak_deps=False --save

# The best way to make sure we have all of condor's dependencies is to install condor.
# XXX Should we skip downloading a condor tarball when running a pilot and just
#     use what's in here?
#     If not, we could uninstall condor if we wanted to save ~30 MB (after the
#     sanity check, see below)
yum install -y condor || fail 66 "Couldn't install condor RPM"

# Get singularity
yum install -y singularity || fail 67 "Couldn't install singularity RPM"
# Make it not setuid since we may not have the privs
sed -i -e '/^allow setuid/s/yes/no/' /etc/singularity/singularity.conf

# Run a singularity sanity check
singularity exec /usr/libexec/condor/exit_37.sif /exit_37;  ret=$?

if [[ $ret != 37 ]]; then
    fail 68 "Singularity sanity check failed: expected code 37, got $ret"
fi

# Cleanup to save space in the container
yum clean all

# vim:et:sw=4:sts=4:ts=8
