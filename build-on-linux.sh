#!/bin/sh

# Build RPMs for Redhat Enterprise Linux and clones
if [ -f /etc/redhat-release ]; then
    # Fake a BUILD-ID
    date +'%Y%m%d%H%M%s' > BUILD-ID
    export TMP='/tmp'
    tar cfz "${TMP}/condor.tar.gz" *
    build/packaging/srpm/nmibuilduwrpm.sh
    exit 0
fi

echo 'Unsupported Linux Release'
exit 1
