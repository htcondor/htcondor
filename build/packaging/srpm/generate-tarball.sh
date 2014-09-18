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
    src/condor_tests/job_vmu_basic.sh \
    src/condor_tests/job_vmu_cdrom.run \
    src/condor_tests/job_vmu_cdrom.sh \
    src/condor_tests/job_vmu_ckpt.run \
    src/condor_tests/job_vmu_ckpt.sh \
    src/condor_tests/job_vmu_network.key \
    src/condor_tests/job_vmu_network.run \
    src/condor_tests/job_vmu_network.sh \
    src/condor_tests/job_vmu_basic.run \
    src/condor_tests/x_vm_utils.pm \
    src/condor_tests/x_param.vmware \
    src/condor_tests/x_vmware_test_vm.cmd \
    src/condor_tests/x_vmware_configpostsrc \
    src/condor_tests/job_ligo_x86-64-chkpttst.cmd \
    src/condor_tests/job_ligo_x86-64-chkpttst.run \
    src/condor_tests/job_quill_basic.cmd \
    src/condor_tests/job_quill_basic.pl \
    src/condor_tests/job_quill_basic.run \
    src/condor_tests/x_job_quill_basic.template \
    src/condor_tests/x_job_quill_supw \
    src/condor_tests/x_param.quill \
    src/condor_tests/x_postgress_quill.conf \
    src/condor_tests/x_quill_buildperlmods.pl \
    src/condor_tests/x_quill_config_postsrc.template \
    src/condor_tests/x_quill_Expect-1.20.tar.gz \
    src/condor_tests/x_quill_IO-Tty-1.07.tar.gz \
    src/condor_tests/x_quill_pgsqlinstall.pl \
    src/condor_tests/x_quill_pgsqlstop.pl \
    src/condor_tests/x_quill_readline-5.2.tar.gz \
    src/condor_tests/x_quill_readlineinstall.pl \
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

