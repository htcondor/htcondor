#!/bin/bash

progdir=${0%/*}

[[ -e $progdir/env.sh ]] && source "$progdir/env.sh"

set -eu

mv bld_external bld_external_ubuntu
mv bld_external_rhel bld_external

if [[ $DOCKER_IMAGE = centos* ]]; then
    yum -y install epel-release
fi
yum -y install ${RPM_DEPENDENCIES[@]}
if [[ $DOCKER_IMAGE = centos:centos7 ]]; then
    yum -y install python36-devel boost169-devel boost169-static
fi
time cmake ${CMAKE_OPTIONS[@]}
time make -j2 externals
time make -j2

# vim:et:sw=4:sts=4:ts=8
