#!/bin/sh

campus_factory_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"


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
cp $campus_factory_dir/passwdfile $local_dir

# Untar the executables
tar xzf $campus_factory_dir/glideinExec.tar.gz

# All late-binding configurations
export CONDOR_CONFIG=$campus_factory_dir/glidein_condor_config
export _condor_LOCAL_DIR=$local_dir
export _condor_SBIN=$local_dir/glideinExec
export _condor_LIB=$local_dir/glideinExec

export LD_LIBRARY_PATH=$_condor_LIB

if [ -e `pwd`/user_job_wrapper.sh ]
then
export _condor_USER_JOB_WRAPPER=$campus_factory_dir/user_job_wrapper.sh
fi

./glideinExec/glidein_startup -dyn -f -r 1200


rm -rf $local_dir

