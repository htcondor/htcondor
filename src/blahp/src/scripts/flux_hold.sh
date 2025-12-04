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

. `dirname $0`/flux_utils.sh

flux_utils_get_binpath flux_binpath

sleep 5s

flux_utils_split_jobid "$1" "requested" "cluster_name"

cmdout=`${flux_binpath}flux job urgency $requested 0 2>&1`
retcode=$?
# NOTE: since Flux doesn't have an equivalent to 'scontrol requeuehold',
#       this script just returns an error if 'flux job urgency' fails.
#       The only real reason for that command to fail is if the job has
#       entered a state where the urgency cannot be changed (e.g., RUN).

if [ "$retcode" == "0" ] ; then
  echo " 0 No\\ error"
  exit 0
else
  echo " 1 Error"
  exit 1
fi
