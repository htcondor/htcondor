#!/bin/bash -v

set -e

echo "=== Job environment ==="
env
echo

echo "=== Contents of credential directory ==="
ls -l ${_CONDOR_CREDS}
echo

echo "=== Contents of credential files ==="
for file in ${_CONDOR_CREDS}/*
do echo "--- ${file} ---" ;
cat ${file}
echo
done
echo

exit 0
