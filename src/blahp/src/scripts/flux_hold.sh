#!/bin/bash

# File:     slurm_hold.sh
# Author:   Jaime Frey (jfrey@cs.wisc.edu)
# Based on code by David Rebatto (david.rebatto@mi.infn.it)
# 
# Copyright (c) Members of the EGEE Collaboration. 2004. 
# Copyright (c) HTCondor Team, Computer Sciences Department,
#   University of Wisconsin-Madison, WI. 2015.
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

if [ -z "$flux_binpath" ] ; then
  flux_binpath=/usr/bin
fi

. `dirname $0`/flux_utils.sh

sleep 5s

flux_utils_check_or_get_requeued_job "$1" "requested" "cluster_name"

cmdout=`${flux_binpath}/flux job urgency $requested 0 2>&1`
retcode=$?
if echo "$cmdout" | grep -q 'job is inactive' || echo "$cmdout" | grep -q 'urgency cannot be changed once resources are allocated' ; then
  # Note: all of this in the conditional is a Flux way to implement the
  #       equivalent to `scontrol requeuehold`
  # First, we cancel the existing job, if it's running.
  ${flux_binpath}/flux cancel $requested
  # Next, we get the jobspec for the original job
  jobspec=`${flux_binpath}/flux job info $requested jobspec`
  retcode=$?
  # If we were successful in getting the jobspec:
  if [ "$retcode" == "0" ]; then
    # Resubmit the job with a hold. This creates a new job ID.
    new_flux_job_id=`${flux_binpath}/flux job submit --urgency=hold ${jobspec}`
    retcode=$?
    # If the resubmission was successful, add a mapping of original job ID to new job ID
    # to the mapping script
    if [ "$retcode" == "0" ]; then
      flux_utils_add_requeued_job "$1" "$requested" "$new_flux_job_id"
      retcode=$?
    fi
  fi
fi

if [ "$retcode" == "0" ] ; then
  echo " 0 No\\ error"
  exit 0
else
  echo " 1 Error"
  exit 1
fi
