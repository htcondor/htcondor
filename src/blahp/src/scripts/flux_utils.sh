#!/bin/bash
#
# File:     flux_utils.sh
# Author:   Ian Lumsden (ilumsden@vols.utk.edu)
# Author:   Michela Taufer (mtaufer@utk.edu)
#
# Flux support was added as part of work supported by the US National Science Foundation
# under Grant No. 2530461, 2513101, 2331152, 2223704, 2138811, and 2103845.
# 
# Flux support was also added under the auspices of the U.S. Department of Energy by
# Lawrence Livermore National Laboratory under Contract DE-AC52-07NA27344
# and was supported by the LLNL-LDRD Program under Project No. 24-SI-005.
#
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

# This function creates a BLAHP job ID from a Flux job ID
# Arguments:
#   1. A Flux job ID returned by `flux batch` 
# Output:
#   * A global variable named "blahp_jobID" containing the BLAHP Job ID
# Returns:
#   None
function flux_utils_create_blahp_jobid {
  local flux_jobid="$1"
  
  local datenow=`date +%Y%m%d`
  
  blahp_jobID=flux/`basename $datenow`/${flux_jobid}
  if [ "$cluster_name" != "" ]; then
    blahp_jobID=${blahp_jobID}@${cluster_name}
  fi
}

# This function splits a BLAHP job ID into a Flux job ID and (optionally) a cluster name
# Arguments:
#   1. A BLAHP job ID returned from the submit command 
#   2. The name of the global variable in which to store the Flux job ID
#   3. The name of the global variable in which to store the cluster name
# Outputs:
#   * Two global variables, named based on inputs, containing the Flux job ID and cluster name
# Returns:
#   None
function flux_utils_split_jobid {
  local blahp_jobid="$1"
  local flux_jobid_varname="$2"
  local flux_cluster_name_varname="$3"

  declare -g "${flux_jobid_varname}=`echo ${blahp_jobid} | sed 's/^.*\///'`"
  
  declare -g "${flux_cluster_name_varname}=`echo ${!flux_jobid_varname} | cut -s -f2 -d@`"
  if [ "${!flux_cluster_name_varname}" != "" ] ; then
    declare -g "${flux_jobid_varname}=`echo ${!flux_jobid_varname} | cut -f1 -d@`"
  fi
}
