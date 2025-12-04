#!/bin/bash

# File:     flux_status.sh
# Author:   Ian Lumsden (ilumsden@vols.utk.edu)
# Author:   Michela Taufer (mtaufer@utk.edu)
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

# Load BLAH config
. `dirname $0`/blah_load_config.sh

. `dirname $0`/flux_utils.sh

# If flux_binpath is specified in the BLAH config, use it.
# Otherwise, set flux_binpath to /usr/bin
flux_utils_get_binpath flux_binpath

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

# Loop over the BLAHP job IDs
for  reqfull in $pars ; do
  # Get the Flux job ID (reqjob) and cluster name (cluster_name) from the
  # current BLAHP job ID (reqfull)
  flux_utils_split_jobid $reqfull "reqjob" "cluster_name"

  # Set a temporary file to store the output of 'flux jobs'
  staterr=/tmp/${reqjob}_staterr
  
  # Use 'flux jobs' to get the requested job information in JSON format
  job_json=`${flux_binpath}flux jobs --json $reqjob 2>$staterr`
  stat_exit_code=$?
  
  # Use a separate 'flux jobs' command to validate the job ID
  # NOTE: this is not really needed for Flux v0.80.0 and later since
  #       'flux jobs' will produce a bad exit code
  ${flux_binpath}flux jobs -no {id} $reqjob 2>/dev/null | grep -q $reqjob
  valid_jobid_exit_code=$?

  # Set default values for blah_status and exit_code
  blah_status=4
  exit_code=0
  
  # If the job ID is valid, proceess the JSON from the first 'flux jobs' command
  if [ $valid_jobid_exit_code -eq 0 ]; then
    # Get the 'status', 'urgency', and 'returncode' fields from the JSON
    job_status=`echo $job_json | jq -r '.["status"]'`
    job_urgency=`echo $job_json | jq -r '.["urgency"]'`
    job_returncode=`echo $job_json | jq -r '.["returncode"]'`
    
    # Convert the Flux job status to a BLAH status code.
    # The possible BLAH status codes are:
    #   * IDLE (1): job is waiting on the scheduler/batch system
    #   * RUNNING (2): job is running on a worker/compute node
    #   * REMOVED (3): job was successfully cancelled
    #   * COMPLETED (4): job has completed its execution, regardless if it was successful or unsuccessful
    #   * HELD (5): job execution is suspended and job is still in the queue
    case $job_status in
      DEPEND)
        blah_status=1
        ;;
      PRIORITY)
        blah_status=1
        ;;
      SCHED)
        blah_status=1
        ;;
      RUN)
        blah_status=2
        ;;
      CLEANUP)
        blah_status=2
        ;;
      COMPLETED)
        blah_status=4
        ;;
      FAILED)
        blah_status=4
        ;;
      CANCELED)
        blah_status=3
        ;;
      TIMEOUT)
        blah_status=3
        ;;
      *)
        blah_status=4
        ;;
    esac
    # In Flux, the HELD state is represented by the job having an urgency of 0.
    # If the job is not yet complete and its urgency is 0, set blah_status to HELD (5)
    if [ $blah_status -ne 4 ] && [ $job_urgency -eq 0 ]; then
      blah_status=5
    fi
    
    # If the job is complete and there is a return code in the JSON, set exit_code
    # to that return code
    if [ $blah_status -eq 4 ] && [ "$job_returncode" != "null" ]; then
        exit_code=$job_returncode
    fi
  fi
  
  # Build the result string
  result_str="[BatchJobId=\"${reqjob}\";JobStatus=${blah_status};"
  if [ $blah_status -eq 4 ]; then
    result_str="${result_str}ExitCode=${exit_code};"
  fi
  result_str="${result_str}]"
  
  # Retrieve the STDERR from the first `flux jobs` command and
  # delete the temporary file
  errout=`cat $staterr`
  rm -f $staterr 2>/dev/null

  # If the job ID is invalid, set 'stat_exit_code' to 0 to produce
  # an output as if the job ID were valid
  if [ $valid_jobid_exit_code -ne 0 ] ; then
    stat_exit_code=0
  fi

  # If the status lookup was successful (as indicated by stat_exit code),
  # print 0 followed by the result string.
  # Otherwise, print "1Error:" followed by the STDERR of the original
  # 'flux jobs' command
  if [ $stat_exit_code -eq 0 ] ; then
    echo 0${result_str}
  else
    echo 1Error: ${errout}
  fi

done

exit 0
