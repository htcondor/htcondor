#!/bin/bash

# Build RPMs and debs for Linux in the current directory using the
# source in the named directory
if [ -f /etc/debian_version -o -f /etc/redhat-release -o -f /etc/system-release ]; then

    # Locate the source directory
    if [ $# -eq 1 -a -d $1 ]; then
        src_dir=$1
    else
        echo "Usage: $0 <source-directory>"
        exit 1
    fi

    # Determine the HTCondor version number
    condor_version=$(awk -F\" '/^set\(VERSION / {print $2}' ${src_dir}/CMakeLists.txt)
    echo "Building HTCondor version ${condor_version}"

    # Fake a BUILD-ID if one not provided
    if [ ! -f BUILD-ID ]; then
        date +'%Y%m%d%H%M' > BUILD-ID
    fi

    # Create the source tarball from the source directory
    mkdir -p condor-${condor_version}
    cp -p ${src_dir}/.??* condor-${condor_version} > /dev/null 2>&1
    cp -pr ${src_dir}/* condor-${condor_version} > /dev/null 2>&1
    tar cfz condor-${condor_version}.tgz condor-${condor_version}
    rm -rf condor-${condor_version}

    # Call the official build scripts
    if [ -f /etc/debian_version ]; then
        exec ${src_dir}/nmi_tools/glue/build/build_uw_deb.sh
    fi
    if [ -f /etc/redhat-release -o -f /etc/system-release ]; then
        exec ${src_dir}/nmi_tools/glue/build/build_uw_rpm.sh
    fi
fi

echo 'Unsupported Linux Release'
exit 1
