#!/bin/bash
#
# 	File:     kubernetes_submit.sh
# 	Author:   Giuseppe Fiorentino (giuseppe.fiorentino@mi.infn.it)
# 	Email:    giuseppe.fiorentino@mi.infn.it
#
# 	Revision history:
# 	08-Aug-2006: Original release
#       27-Oct-2009: Added support for 'local' requirements file.
#
# 	Description:
#   	Submission script for Condor, to be invoked by blahpd server.
#   	Usage:
#  	condor_submit.sh -c <command> [-i <stdin>] [-o <stdout>] [-e <stderr>] [-w working dir] [-- command's arguments]
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

usage_string="Usage: $0 -c <command> [-i <stdin>] [-o <stdout>] [-e <stderr>] [-v <environment>] [-s <yes | no>] [-- command_arguments]"

workdir=$PWD

###############################################################
# Parse parameters
###############################################################
original_args="$@"
# Note: -s (stage command) s ignored as it is not relevant for Condor.

# script debug flag: currently unused
debug=no

# number of MPI nodes: interpretted as a core count for vanilla universe
mpinodes=1

# Name of local requirements file: currently unused
req_file=""

while getopts "a:i:o:de:j:n:N:z:h:S:v:V:c:w:x:u:q:r:s:T:I:O:R:C:D:m:A:t:" arg 
do
    case "$arg" in
    a) xtra_args="$OPTARG" ;;
    i) stdin="$OPTARG" ;;
    o) stdout="$OPTARG" ;;
    d) debug="yes" ;;
    e) stderr="$OPTARG" ;;
    j) creamjobid="$OPTARG" ;;
    v) envir="$OPTARG";;
    V) environment="$OPTARG";;
    c) command="$OPTARG" ;;
    n) mpinodes="$OPTARG" ;;
    N) hostsmpsize="$OPTARG";;
    z) wholenodes="$OPTARG";;
    h) hostnumber="$OPTARG";;
    S) smpgranularity="$OPTARG";;
    w) workdir="$OPTARG";;
    x) proxy_file="$OPTARG" ;;
    u) proxy_subject="$OPTARG" ;;
    q) queue="$OPTARG" ;;
    r) dummy_proxyrenew="$OPTARG" ;;
    s) stgcmd="$OPTARG" ;;
    T) temp_dir="$OPTARG" ;;
    I) inputflstring="$OPTARG" ;;
    O) outputflstring="$OPTARG" ;;
    R) remaps="$OPTARG" ;;
    C) req_file="$OPTARG" ;;
    D) run_dir="$OPTARG" ;;
    m) req_mem="$OPTARG" ;;
    A) project="$OPTARG" ;;
    t) runtime="$OPTARG" ;;
    -) break ;;
    ?) echo $usage_string
       exit 1 ;;
    esac
done

if [ -z "$temp_dir"  ] ; then
      curdir=`pwd`
      temp_dir="$curdir"
fi

shift `expr $OPTIND - 1`
arguments=$*


# Command is mandatory
if [ "x$command" == "x" ]
then
    echo $usage_string
    exit 1
fi

# Move into the IWD so we don't clutter the current working directory.
curdir=`pwd`
if [ "x$workdir" == "x" ]; then
    if [ "x$blah_set_default_workdir_to_home" == "xyes" ]; then
        workdir=$HOME
    fi
fi

if [ "x$workdir" != "x" ]; then
    cd $workdir
    if [ $? -ne 0 ]; then
	echo "Failed to CD to Initial Working Directory." >&2
	echo Error # for the sake of waiting fgets in blahpd
	exit 1
    fi
fi

# These are the extra, k8s specific parameters
if [ -r "$req_file" ]; then
	k8s_image=$(awk -F= '{if ($1 == "k8s_image") {print $2}}' $req_file | tr -d "'\"")
	k8s_cpus=$(awk -F= '{if ($1 == "RequestCpus") {print $2}}' $req_file | tr -d "'\"")
	k8s_memory=$(awk -F= '{if ($1 == "RequestMemory") {print $2}}' $req_file | tr -d "'\"")
	k8s_disk=$(awk -F= '{if ($1 == "RequestDisk") {print $2}}' $req_file | tr -d "'\"")
	k8s_namespace=$(awk -F= '{if ($1 == "k8s_namespace") {print $2}}' $req_file | tr -d "'\"")
fi

if [ -z "${k8s_image}" ]; then
	echo "No k8s_image in submit file" >&2
	echo Error
	exit 1
fi

if [ -z "${k8s_cpus}" ]; then
	k8s_cpus=1
fi

if [ -z "${k8s_memory}" ]; then
	k8s_memory=1024
fi

if [ -z "${k8s_disk}" ]; then
	k8s_disk=1024
fi

if [ -z "${k8s_namespace}" ]; then
	k8s_namespace=default
fi

##############################################################
# Create submit file
###############################################################

if [ ! -z "$run_dir" ] ; then
    pod_name=`echo $run_dir | sed -e 's/[^#]*#/condor-/' -e 's/[^-A-Za-z0-9]/-/g'`
else
    # We need a unique identifier, and didn't get a good candidate.
    # Poor fallback is to use our pid.
    pod_name="condor-$$"
fi

submit_file=`mktemp -q $temp_dir/blahXXXXXX`
if [ $? -ne 0 ]; then
    echo "mktemp failed" >&2
    echo Error
    exit 1
fi

#  Remove any preexisting submit file
rm -f $submit_file

if [ ! -z "$inputflstring" ] ; then
    i=0
    for file in `cat $inputflstring`; do
	input_files[$i]=$file
	i=$((i+1))
    done
fi

if [ ! -z "$outputflstring" ] ; then
    i=0
    for file in `cat $outputflstring`; do
	output_files[$i]=$file
	i=$((i+1))
    done
fi

if [ ! -z "$remaps" ] ; then
    i=0
    for file in `cat $remaps`; do
	remap_files[$i]=$file
	i=$((i+1))
    done
fi

if [ ${#input_files[@]} -gt 0 ] ; then
    transfer_input_files="transfer_input_files=${input_files[0]}"
    for ((i=1; i < ${#input_files[@]}; i=$((i+1)))) ; do
	transfer_input_files="$transfer_input_files,${input_files[$i]}"
    done
fi

if [ ${#output_files[@]} -gt 0 ] ; then
    transfer_output_files="transfer_output_files=${output_files[0]}"
    for ((i=1; i < ${#output_files[@]}; i=$((i+1)))) ; do
	transfer_output_files="$transfer_output_files,${output_files[$i]}"
    done
fi

if [ ${#remap_files[@]} -gt 0 ] ; then
    if [ ! -z "${remap_files[0]}" ] ; then
	map=${remap_files[0]}
    else
	map=${output_files[0]}
    fi
    transfer_output_remaps="transfer_output_remaps=\"${output_files[0]}=$map"
    for ((i=1; i < ${#remap_files[@]}; i=$((i+1)))) ; do
	if [ ! -z "${remap_files[0]}" ] ; then
	    map=${remap_files[$i]}
	else
	    map=${output_files$i]}
	fi
	transfer_output_remaps="$transfer_output_remaps;${output_files[$i]}=$map"
    done
    transfer_output_remaps="$transfer_output_remaps\""
fi

# Convert input environment (old Condor or shell format as dictated by 
# input args):

submit_file_environment="#"

if [ "x$environment" != "x" ] ; then
# Input format is suitable for bourne shell style assignment. Convert to
# old condor format (no double quotes in submit file).
# FIXME: probably it's better to convert everything into the 'new' Condor
# environment format.
    eval "env_array=($environment)"
    submit_file_environment=""
    for  env_var in "${env_array[@]}"; do
        if [ "x$submit_file_environment" == "x" ] ; then
            submit_file_environment="environment = "
        else
            submit_file_environment="$submit_file_environment;"
        fi
        submit_file_environment="${submit_file_environment}${env_var}"
    done
else
    if [ "x$envir" != "x" ] ; then
# Old Condor format (no double quotes in submit file)
        submit_file_environment="environment = $envir"
    fi
fi

### This appears to only be necessary if Condor is passing arguments
### with the "new_esc_format"
### If the first and last characters are '"', then assume that is the case.
# # NOTE: The arguments we are given are specially escaped for a shell,
# # so to get them back into Condor format we need to remove all the
# # extra quotes. We do this by replacing '" "' with ' ' and stripping
# # the leading and trailing "s.
# TODO This needs works, particularly for the new-style escaping
if echo $arguments | grep -q '^".*"$' ; then
  arguments=$(echo $arguments | sed -e "s/\" \"/', '/g")
  arguments="'${arguments:1:$((${#arguments}-2))}'"
elif [ "$arguments" != "" ] ; then
  arguments="'"$(echo $arguments | sed -e "s/ /', '/g")"'"
fi

#if [ "x$proxy_file" != "x" ]
#then
#  echo "x509userproxy = $proxy_file" >> $submit_file
#fi

#if [ "x$xtra_args" != "x" ]
#then
#  echo -e $xtra_args >> $submit_file
#fi

#if [ "x$req_mem" != "x" ]
#then
#  echo "request_memory = $req_mem" >> $submit_file
#fi

#if [ "x$runtime" != "x" ]
#then
#  echo "periodic_remove = JobStatus == 2 && time() - JobCurrentStartExecutingDate > $runtime" >> $submit_file
#fi

cat > $submit_file << EOF
apiVersion: v1
kind: Pod
metadata:
 name: ${pod_name}
 namespace: ${k8s_namespace}
spec:
 restartPolicy: Never
 volumes:
 - name: glidein-wn
   configMap:
     name: glidein-wn
 containers:
  - name: myapp-container
    image: ${k8s_image}
    command: [ "${command}" ]
    args: [ $arguments ]
    resources:
      limits:
        cpu: ${k8s_cpus}
        memory: ${k8s_memory}M
        ephemeral-storage: ${k8s_disk}M
      requests:
        cpu: ${k8s_cpus}
        memory: ${k8s_memory}M
        ephemeral-storage: ${k8s_disk}M
    volumeMounts:
    - name: glidein-wn
      mountPath: /root/config/10-security.cfg
      subPath: 10-security.cfg
    env:
    - name: CONDOR_HOST
      value: glidein2.chtc.wisc.edu
EOF


###############################################################
# Perform submission
###############################################################

full_result=$(kubectl create -f $submit_file)
return_code=$?

if [ "$return_code" == "0" ] ; then
    blahp_jobID="kubernetes/$pod_name"

    echo "BLAHP_JOBID_PREFIX$blahp_jobID"
else
    echo "Failed to submit"
    echo Error
fi
cp $submit_file /tmp/k8s_sub.$$

# Clean temporary files -- There only temp file is the one we submit
rm -f $submit_file

exit $return_code
