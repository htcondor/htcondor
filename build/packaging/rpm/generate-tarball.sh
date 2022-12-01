#!/bin/bash

input="condor_src-$1-all-all.tar.gz"

if [ ! -f $input ] ; then
   echo "$0: $input is not a regular file";
   exit 1;
fi

echo "Processing $input"

echo "...extracting $input"
tar xzf $input

#cd condor_src-$1
cd condor-$1

if [ ! -f BUILD-ID ] ; then
   build="UNKNOWN"
else
   build=`cat BUILD-ID`
fi

echo "...recording BUILD-ID: $build"

for f in \
    src/condor_mail \
    src/condor_tests/job_ligo_x86-64-chkpttst.cmd \
    src/condor_tests/job_ligo_x86-64-chkpttst.run \
    condor_src-7.6.0-all-all.tar.gz;
do
    echo "...removing $f";
    rm -r $f;
done;

#echo "...removing all externals except 'man'"
#mv externals/bundles/man externals/
#rm -r externals/bundles/*
#mv externals/man externals/bundles/

echo "...creating condor-$1-$build-RH.tar.gz"
cd ..
#mv condor_src-$1 condor-$1
tar czfsp condor-$1-$build-RH.tar.gz condor-$1

echo "...cleaning up"
rm -rf condor-$1

