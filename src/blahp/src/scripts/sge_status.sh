#!/bin/bash

# File:     sge_status.sh
#
# Copyright (c) Members of the EGEE Collaboration. 2004. 
# See http://www.eu-egee.org/partners/ for details on the copyright
# holders.  
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

sge_helper_path=${blah_libexec_directory}

usage_string="Usage: $0 [-w] [-n]"

#get worker node info
getwn=""

#get creamport
getcreamport=""

###############################################################
# Parse parameters
###############################################################

while getopts "wn" arg 
do
    case "$arg" in
    w) getwn="--getworkernodes" ;;
    n) getcreamport="yes" ;;
    
    -) break ;;
    ?) echo $usage_string
       exit 1 ;;
    esac
done

shift `expr $OPTIND - 1`

if [ "x$getcreamport" == "xyes" ]
then
    exec ${blah_sbin_directory}/blah_job_registry_lkup -n
fi

if [ -z "$sge_rootpath" ]; then sge_rootpath="/usr/local/sge/pro"; fi
if [ -r "$sge_rootpath/${sge_cellname:-default}/common/settings.sh" ]
then
  . $sge_rootpath/${sge_cellname:-default}/common/settings.sh
fi

tmpid=`echo "$@"|sed 's/.*\/.*\///g'`

# ASG Keith way
jobid=${tmpid}.${sge_cellname:-default}


blahp_status=`exec ${sge_helper_path}/sge_helper --status $getwn $jobid`
retcode=$?

# Now see if we need to run qstat 'manually'
if [ $retcode -ne 0 ]; then

   qstat_out=`qstat`

   # First, find the column with the State information:
   state_col=`echo "$qstat_out" | head -n 1 | awk '{ for (i = 1; i<=NF; i++) if ($i == "state") print i;}'`
   job_state=`echo "$qstat_out" | awk -v "STATE_COL=$state_col" -v "JOBID=$tmpid" '{ if ($1 == JOBID) print $STATE_COL; }'`
  
   if [[ "$job_state" =~  q ]]; then
      jobstatus=1
   elif [[ "$job_state" =~ [rt] ]]; then
      jobstatus=2
   elif [[ "$job_state" =~ h ]]; then
      jobstatus=5
   elif [[ "$job_state" =~ E ]]; then
      jobstatus=4
   elif [[ "$job_state" =~ d ]]; then
      jobstatus=3
   elif [ "x$job_state" == "x" ]; then
      jobstatus=4
   fi

   if [ $jobstatus -eq 4 ]; then
      blahp_status="[BatchJobId=\"$tmpid\";JobStatus=$jobstatus;ExitCode=0]"
   else
      blahp_status="[BatchJobId=\"$tmpid\";JobStatus=$jobstatus]"
   fi
   retcode=0

fi

echo ${retcode}${blahp_status}
#exit $retcode
