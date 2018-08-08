#!/bin/bash

# File:     slurm_status.sh
# Author:   Jaime Frey (jfrey@cs.wisc.edu)
# Based on code by David Rebatto (david.rebatto@mi.infn.it)
# 
# Licensed under the Apache License, Version 2.0 (the "License"); 
# you may not use this file except in compliance with the License. 
# You may obtain a copy of the License at 
# 
#     http://www.apache.org/licenses/LICENSE-2.0 
# 
# Unless required by applicable law or agreed to in writing, software 
# distributed under the License is distributed on an "AS IS" BASIS, 
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
# See the License for the specific language governing permissions and 
# limitations under the License.
#

. `dirname $0`/blah_load_config.sh

if [ -x ${blah_bin_directory}/slurm_status.py ] ; then
    exec ${blah_bin_directory}/slurm_status.py "$@"
fi

if [ -z "$slurm_binpath" ] ; then
  slurm_binpath=/usr/bin
fi

usage_string="Usage: $0 [-w] [-n]"

###############################################################
# Parse parameters
###############################################################

while getopts "wn" arg 
do
    case "$arg" in
    w) getwn="yes" ;;
    n) ;;
    
    -) break ;;
    ?) echo $usage_string
       exit 1 ;;
    esac
done

shift `expr $OPTIND - 1`

pars=$*
proxy_dir=~/.blah_jobproxy_dir

for  reqfull in $pars ; do
  reqjob=`echo $reqfull | sed -e 's/^.*\///'`

  staterr=/tmp/${reqjob}_staterr

  result=`${slurm_binpath}/scontrol show job $reqjob 2>$staterr`
  stat_exit_code=$?
  result=`echo "$result" | awk -v job_id=$reqjob -v proxy_dir=$proxy_dir '
BEGIN {
    blah_status = 4
    slurm_status = ""
    exit_code = "0"
}

/JobState=/ {
    slurm_status = substr( $1, index( $1, "=" ) + 1 )
}

/ExitCode=/ {
    if ( split( $4, tmp, "[=:]" ) == 3 ) {
        exit_code = tmp[2]
    }
}

END {
    if ( slurm_status ~ "BOOT_FAIL" ) { blah_status = 4 }
    if ( slurm_status ~ "CANCELLED" ) { blah_status = 3 }
    if ( slurm_status ~ "COMPLETED" ) { blah_status = 4 }
    if ( slurm_status ~ "CONFIGURING" ) { blah_status = 1 }
    if ( slurm_status ~ "COMPLETING" ) { blah_status = 2 }
    if ( slurm_status ~ "FAILED" ) { blah_status = 4 }
    if ( slurm_status ~ "NODE_FAIL" ) { blah_status = 4 }
    if ( slurm_status ~ "PENDING" ) { blah_status = 1 }
    if ( slurm_status ~ "PREEMPTED" ) { blah_status = 4 }
    if ( slurm_status ~ "RUNNING" ) { blah_status = 2 }
    if ( slurm_status ~ "SPECIAL_EXIT" ) { blah_status = 4 }
    if ( slurm_status ~ "STOPPED" ) { blah_status = 2 }
    if ( slurm_status ~ "SUSPENDED" ) { blah_status = 2 }
    if ( slurm_status ~ "TIMEOUT" ) { blah_status = 4 }

    print "[BatchJobId=\"" job_id "\";JobStatus=" blah_status ";"
    if ( blah_status == 4 ) {
        print "ExitCode=" exit_code ";"
    }
    print "]\n"
    if ( blah_status == 3 || blah_status == 4 ) {
        #system( "rm " proxy_dir "/" job_id ".proxy 2>/dev/null" )
    }
}
'
`

  errout=`cat $staterr`
  rm -f $staterr 2>/dev/null

  if echo "$errout" | grep -q "Invalid job id specified" ; then
    stat_exit_code=0
  fi
  if [ $stat_exit_code -eq 0 ] ; then
    echo 0${result}
  else
    echo 1Error: ${errout}
  fi

done

exit 0
