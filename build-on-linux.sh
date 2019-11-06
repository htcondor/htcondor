#!/bin/bash

# Build RPMs for Redhat Enterprise Linux and clones
if [ -f /etc/redhat-release -o -f /etc/debian_version ]; then
    condor_version=$(awk -F\" '/^set\(VERSION / {print $2}' CMakeLists.txt)
    # Fake a BUILD-ID
    date +'%Y%m%d%H%M' > BUILD-ID
    buildid=$(<BUILD-ID)
    mkdir -p /tmp/$buildid/condor-$condor_version
    cp -p .??* /tmp/$buildid/condor-$condor_version
    cp -pr * /tmp/$buildid/condor-$condor_version
    src_dir=$PWD
    cd /tmp/$buildid
    tar cfz $src_dir/condor-${condor_version}.tgz condor-${condor_version}
    cd $src_dir
    rm -rf /tmp/$buildid
    if [ -f /etc/redhat-release ]; then
        nmi_tools/glue/build/build_uw_rpm.sh
    fi
    if [ -f /etc/debian_version ]; then
        nmi_tools/glue/build/build_uw_deb.sh
    fi
    rm BUILD-ID
    exit 0
fi

echo 'Unsupported Linux Release'
exit 1
