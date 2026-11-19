#!/bin/bash
#
# File:     sfapi_status.sh
# Author:   Karan Vahi (vahi@isi.edu)
#
# Description:
#   Status script for jobs submitted to Perlmutter via the NERSC SFAPI,
#   to be invoked by blahpd server.
#   Queries job state via sfapi_helpers.py and maps Slurm/SFAPI states to
#   blahp JobStatus codes:
#     1 = Idle/Queued   2 = Running   3 = Cancelled   4 = Done/Failed
#   Usage:
#     sfapi_status.sh [-w] [-n] sfapi/<date>/<jobid> [sfapi/<date>/<jobid> ...]
#
# Copyright (c) HTCondor Team, Computer Sciences Department,
#   University of Wisconsin-Madison, WI. 2024.
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
. `dirname $0`/sfapi_setup.sh

sfapi_helpers_dir=`dirname $0`

usage_string="Usage: $0 [-w] [-n] sfapi/<date>/<jobid> [sfapi/<date>/<jobid> ...]"

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

###############################################################
# Map a Slurm/SFAPI job state string to a blahp JobStatus number.
# Handles both plain strings (RUNNING) and str-enum reprs
# (JobState.RUNNING) from the sfapi_client library.
###############################################################

map_sfapi_state_to_blahp() {
    # Strip optional "EnumClass." prefix, uppercase for safe comparison
    local state
    state=$(echo "$1" | sed 's/.*\.//' | tr '[:lower:]' '[:upper:]' | tr -d '[:space:]')

    case "$state" in
        PENDING|CONFIGURING)
            echo 1 ;;
        RUNNING|COMPLETING|STOPPED|SUSPENDED)
            echo 2 ;;
        CANCELLED)
            echo 3 ;;
        COMPLETED|FAILED|BOOT_FAIL|NODE_FAIL|PREEMPTED|SPECIAL_EXIT|TIMEOUT|NOT_FOUND|OUT_OF_MEMORY)
            echo 4 ;;
        *)
            echo 1 ;;  # unknown state treated as queued
    esac
}

###############################################################
# Map a Slurm/SFAPI job state string to a exitcode to return
# in BLAHP. Exitcode is only determined if blahp_status is
# either 3 or 4
###############################################################

map_sfapi_state_to_exitcode() {
    # Strip optional "EnumClass." prefix, uppercase for safe comparison
    local state
    state=$(echo "$1" | sed 's/.*\.//' | tr '[:lower:]' '[:upper:]' | tr -d '[:space:]')

    case "$state" in
        COMPLETED)
            echo 0 ;;
        CANCELLED|FAILED|BOOT_FAIL|NODE_FAIL|PREEMPTED|SPECIAL_EXIT|TIMEOUT|NOT_FOUND|OUT_OF_MEMORY)
            echo 1 ;;
    esac
}

###############################################################
# Query each requested job ID
###############################################################
sfapi_state_dir="${HOME}/.blah/sfapi_jobs"
for reqfull in $pars ; do
    # Strip path prefix: sfapi/YYYYMMDD/JOBID -> JOBID
    reqjob=`echo $reqfull | sed -e 's/^.*\///'`

    out=$(python3 "$sfapi_helpers_dir/sfapi_helpers.py" status \
        --type job \
        --value "$reqjob" 2>&1)
    retcode=$?

    if [ "$retcode" != "0" ] ; then
        # check for unknown job error
        job_not_found=$(echo "$out" | grep "SfApiError: Job not found:")
        if [ -n "$job_not_found" ] ; then
            # we are adding our own state
            sfapi_state="NOT_FOUND"
        else
          echo "1Error: $out"
          continue
        fi
    fi

    if [ "x$sfapi_state" == "x" ] ; then
      # Expected output line: "Job <jobid> state: <state>"
      sfapi_state=$(echo "$out" | grep -i "^Job.*state:" | awk -F'state: ' '{print $2}')
    fi

    if [ -z "$sfapi_state" ] ; then
        echo "1Error: could not parse job state from: $out"
        continue
    fi

    blahp_status=$(map_sfapi_state_to_blahp "$sfapi_state")

    # this is the path that sfapi_submit.sh and sfapi_helpers.py refer
    # as the jobstate file. We map the blahp job id to the file
    # sfapi/20260511/52833290 -> 20260511_52833290
    jobstate_base=`echo ${reqfull} | sed 's/^sfapi\///' | sed 's/\//_/g'`
    jobstate_file="${sfapi_state_dir}/${jobstate_base}"

    # Download remote output files when the job is done (status 4)
    if [ "$blahp_status" == "4" ] ; then
        dl_out=$(python3 "$sfapi_helpers_dir/sfapi_helpers.py" download "$reqfull" 2>&1)
        if [ "$?" != "0" ] ; then
            echo "1Error: job $reqjob is done but output download mentioned in {$reqfull} failed: $dl_out" >&2
        else
            del_out=$(python3 "$sfapi_helpers_dir/sfapi_helpers.py" delete "$reqfull" 2>&1)
            if [ "$?" != "0" ] ; then
                echo "Warning: job $reqjob output downloaded but remote directory deletion failed: $del_out" >&2
            fi
        fi

        # cleanup the job state file ~/.blah/sfapi_jobs
        if [ -f "${jobstate_file}" ] ; then
            rm -f ${jobstate_file}
        fi

        # TODO: Ask Jaime how to trigger failure / job hold here
    fi

    result="[BatchJobId=\"$reqjob\";JobStatus=${blahp_status};"
    if [ "$blahp_status" == "4" ] || [ "$blahp_status" == "3" ] ; then
        exit_code=$(map_sfapi_state_to_exitcode "$sfapi_state")
        result="${result}ExitCode=${exit_code};"
    fi
    result="${result}]"

    echo "0${result}"
done

exit 0
