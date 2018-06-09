#!/bin/sh

# Build RPMs for Redhat Enterprise Linux and clones
if [ -f /etc/redhat-release -o -f /etc/debian_version ]; then
    # Fake a BUILD-ID
    date +'%Y%m%d%H%M' > BUILD-ID
    export TMP='/tmp'
    tar cfz "${TMP}/condor.tar.gz" *
    if [ -f /etc/redhat-release ]; then
        nmi_tools/glue/build/build_uw_rpm.sh
    fi
    if [ -f /etc/debian_version ]; then
        nmi_tools/glue/build/build_uw_deb.sh
    fi
    exit 0
fi

echo 'Unsupported Linux Release'
exit 1
