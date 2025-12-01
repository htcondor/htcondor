#!/bin/bash
#
# File:     flux_utils.sh
# Author:   Ian Lumsden (ilumsden@vols.utk.edu)
# Author:   Michela Taufer (mtaufer@utk.edu)
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

################################  NOTICE  ###################################################
# The remaining functions in this file are needed to work around the lack of a Flux
# equivalent to `scontrol requeuehold`, which changes a job's state back to PENDING
# and then places a hold on the job. In Flux, job IDs cannot be reused, preventing Flux
# from having an equivalent command. To work around this, whenever `scontrol requeuehold`
# would be called, we will instead create or append to a script in the job's CWD. This script
# will define a Bash associative array where the original BLAHP job IDs (i.e., from BLAHP's
# submit command) are keys and new BLAHP job IDs representing the newly submitted job (i.e.,
# the "requeued" job) are values.
#
# These functions provide various utilities for interacting with these scripts.
################################  NOTICE  ###################################################

# This function gets the path to the job ID mapping script for a given Flux job ID
# by querying the CWD for the Flux job ID
# Arguments:
#   1. The Flux job ID corresponding to a possible key in the mapping
# Outputs:
#   * A global variable named "flux_requeued_map_script" containing the path to the requested mapping script
# Returns:
#   1 if `flux job info` fails and 0 otherwise
function flux_utils_get_requeued_filepath {
  local flux_jobid="$1"

  if [ -z "$flux_binpath" ] ; then
    flux_binpath=/usr/bin
  fi

  local flux_jobid_dir=`${flux_binpath}/flux job info ${flux_jobid} jobspec | jq -r '.attributes.system.cwd'`
  local flux_cwd_check_status=$?
  
  if [[ "$flux_cwd_check_status" -ne 0 ]]; then
    return 1
  fi
  
  flux_requeued_map_script="${flux_jobid_dir}/flux_requeued_map.sh"
  
  return 0
}

# This function creates the job ID mapping script for the specified Flux job ID
# Arguments:
#   1. The Flux job ID corresponding to a possible key in the mapping
# Outputs:
#   * None
# Returns:
#   1 if `flux job info` fails and 0 otherwise
function flux_utils_create_requeued_map_script {
  flux_utils_get_requeued_filepath $1
  local flux_cwd_check_status=$?
  
  if [[ "$flux_cwd_check_status" -ne 0 ]]; then
    return 1
  fi
  
  cat > $flux_requeued_map_script <<EOF
#!/bin/bash

declare -A flux_requeued_map
EOF
  return 0
}

# This function checks if the job ID mapping script for the specified Flux job ID exists
# Arguments:
#   1. The Flux job ID corresponding to a possible key in the mapping
# Outputs:
#   * None
# Returns:
#   2 if `flux job info` fails, 1 if the script does not exist, and 0 otherwise
function flux_utils_check_for_requeued_map_script {
  flux_utils_get_requeued_filepath $1
  local flux_cwd_check_status=$?
  
  if [[ "$flux_cwd_check_status" -ne 0 ]]; then
    return 2
  fi
  
  if [[ ! -f $flux_requeued_map_script ]]; then
    return 1
  else
    return 0
  fi
}

# This function gets the job ID map for the specified Flux job ID by sourcing
# its corresponding script
# Arguments:
#   1. The Flux job ID corresponding to a possible key in the mapping
# Outputs:
#   * A global variable named "flux_requeued_map" containing the job ID mapping
# Returns:
#   1 if the script path could not be obtained or the script does not exist, 0 otherwise
function flux_utils_get_requeued_map {
  flux_utils_get_requeued_filepath $1
  local flux_cwd_check_status=$?
  
  if [[ "$flux_cwd_check_status" -ne 0 ]] || [[ ! -f $flux_requeued_map_script ]]; then
    return 1
  fi
  
  . $flux_requeued_map_script
  
  return 0
}

# This function adds a BLAHP job ID pair to the map corresponding to the original
# Flux job ID
# Arguments:
#   1. The BLAHP job ID that will be used as a key in the mapping
#   2. The Flux job ID corresponding to argument 1
#   3. The Flux job ID corresponding to the requeued job. This will be converted into
#      the BLAHP job ID that will be used as the value in the mapping
# Outputs:
#   None
# Returns:
#   1 if the script path could not be obtained, 0 otherwise
# Note:
#   This function does NOT reload the job ID mapping. It only updates the script
#   defining the mapping.
function flux_utils_add_requeued_job {
  local blahp_original_jobid="$1"
  local flux_original_jobid="$2"
  local flux_requeued_jobid="$3"
  
  flux_utils_get_requeued_filepath $flux_original_jobid
  local flux_cwd_check_status=$?
  
  if [[ "$flux_cwd_check_status" -ne 0 ]]; then
    return 1
  fi
  
  if [[ ! -f $flux_requeued_map_script ]]; then
    flux_utils_create_requeued_map_script $flux_original_jobid
  fi
  
  flux_utils_create_blahp_jobid "$flux_requeued_jobid"
  
  echo "flux_requeued_map[\"${blahp_original_jobid}\"]=\"${blahp_jobID}\"" >> $flux_requeued_map_script
  return 0
}

# This function gets the ID of a requeued job from the mapping
# Flux job ID
# Arguments:
#   1. The BLAHP job ID that is searched as a key in the mapping
#   2. The Flux job ID corresponding to argument 1
# Outputs:
#   * A global variable named "blahp_requeued_jobid" containing the job ID
#     obtained from the mapping
# Returns:
#   1 if the mapping could not be loaded or the input BLAHP job ID is not in
#   the mapping, 0 otherwise
function flux_utils_get_requeued_job {
  local blahp_original_jobid="$1"
  local flux_original_jobid="$2"
  
  flux_utils_get_requeued_map "$flux_original_jobid"
  local get_requeued_map_status=$?

  if [[ "$get_requeued_map_status" -ne 0 ]]; then
    return 1
  fi
  
  blahp_requeued_jobid=
  if [[ " ${!flux_requeued_map[@]} " =~ " ${blahp_original_jobid} " ]]; then
    blahp_requeued_jobid=${flux_requeued_map["${blahp_original_jobid}"]}
    return 0
  else
    return 1
  fi
}

# This function generates the correct Flux job ID and cluster name based on
# the mapping corresponding to the input BLAHP job ID
# Arguments:
#   1. The BLAHP job ID that is searched as a key in the mapping
#   2. The name of the variable into which the Flux job ID will be stored 
#   3. The name of the variable into which the cluster name will be stored 
# Outputs:
#   * Two global variables, named based on inputs, containing the Flux job ID and cluster name
# Returns:
#   0
function flux_utils_check_or_get_requeued_job {
  local blahp_original_jobid="$1"
  local output_flux_jobid_varname="$2"
  local output_cluster_name_varname="$3"
  
  flux_utils_split_jobid "${blahp_original_jobid}" "${output_flux_jobid_varname}" "${output_cluster_name_varname}"
  
  flux_utils_get_requeued_job "${blahp_original_jobid}" "${!output_flux_jobid_varname}"
  local get_requeued_job_status=$?
  
  if [[ "$get_requeued_job_status" -eq 0 ]]; then
    flux_utils_split_jobid "${blahp_requeued_jobid}" "${output_flux_jobid_varname}" "${output_cluster_name_varname}"
  fi
  
  return 0
}