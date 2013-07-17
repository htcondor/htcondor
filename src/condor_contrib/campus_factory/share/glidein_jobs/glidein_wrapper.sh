#!/bin/sh 

starting_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
eval campus_factory_dir=$_campusfactory_CAMPUSFACTORY_LOCATION

# BLAHP does weird things with home directory
unset HOME
export HOME

# Make the temporary directory
if [ ! -d $_campusfactory_wntmp ]
then
  # See if we can make the designated directory
  mkdir -p $_campusfactory_wntmp
fi
local_dir=`mktemp -d -t -p $_campusfactory_wntmp`
cd $local_dir

# Copy the exec tar file
cp $campus_factory_dir/glideinExec.tar.gz $local_dir
cp $starting_dir/passwdfile $local_dir

# Untar the executables
tar xzf $campus_factory_dir/glideinExec.tar.gz

# All late-binding configurations
export CONDOR_CONFIG=$starting_dir/glidein_condor_config
export _condor_LOCAL_DIR=$local_dir
export _condor_SBIN=$local_dir/glideinExec
export _condor_LIB=$local_dir/glideinExec

export LD_LIBRARY_PATH=$_condor_LIB

# Copy the user job wrapper
if [ -e $starting_dir/user_job_wrapper.sh ]
then
cp $starting_dir/user_job_wrapper.sh `pwd`
fi

if [ -e `pwd`/user_job_wrapper.sh ]
then
export _condor_USER_JOB_WRAPPER=`pwd`/user_job_wrapper.sh
fi

./glideinExec/glidein_startup -dyn -f -r 1200


rm -rf $local_dir

