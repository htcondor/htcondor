#!/usr/bin/bash
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

# shellcheck source=./blah_load_config.sh
. "$(dirname "$0")"/blah_load_config.sh
# shellcheck source=./iri_setup.sh
. "$(dirname "$0")"/iri_setup.sh

bls_iri_scripts_log $(/bin/date) status "$@"

iri_helpers_dir=$(dirname "$0")

usage_string="Usage: $0 [-w] [-n] iri/<service>/<jobid> [iri/<service>/<jobid> ...]"

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

map_iri_state_to_blahp() {
    local state
    state=$1

    case "$state" in
        queued)
            echo 1 ;;
        active)
            echo 2 ;;
        canceled)
            echo 3 ;;
        completed|failed)
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

# TODO get exit code (and other info) from iri_helpers.py status command
map_iri_state_to_exitcode() {
    local state
    state="$1"

    case "$state" in
        completed)
            echo 0 ;;
        failed|canceled)
            echo 1 ;;
    esac
}

###############################################################
# Query each requested job ID
###############################################################
iri_state_dir="${HOME}/.blah/iri_jobs"
for reqfull in $pars ; do
    # Break apart blahp id "iri/<service>/<job-id>"
    tmp=${reqfull#*/}
    job_id=${tmp##*/}
    service=${tmp%%/*}

    if ! out=$(python3 "$iri_helpers_dir/iri_helpers.py" "$service" status "$job_id") ; then
        # check for unknown job error
        job_not_found=$(echo "$out" | grep "IriApiError: Job not found:")
        if [ -n "$job_not_found" ] ; then
            # we are adding our own state
            iri_state="not_found"
        else
          echo "1Error: $out"
          continue
        fi
    fi

    if [[ -z "$iri_state" ]] ; then
      # Expected output line: "<state>"
      # TODO Allow additional information in output
      iri_state=${out%% *}
      if [[ "$out" == *" "* ]]; then
        extra_info=${out#* }
      fi
    fi

    if [[ -z "$iri_state" ]] ; then
        echo "1Error: could not parse job state from: $out"
        continue
    fi

    blahp_status=$(map_iri_state_to_blahp "$iri_state")

    # this is the path that iri_submit.sh and iri_helpers.py refer
    # as the jobstate file. We map the blahp job id to the file
    # iri/<service>/52833290 -> <service>_52833290
    jobstate_file="${iri_state_dir}/${service}_${job_id}"

    # Download remote output files when the job is done (status 4)
    if [[ "$blahp_status" == "4" ]] ; then
        if ! dl_out=$(python3 "$iri_helpers_dir/iri_helpers.py" "$service" download-output "$job_id" 2>&1) ; then
            echo "1Error: job $job_id is done but output download mentioned in {$reqfull} failed: $dl_out" >&2
        else
            if ! del_out=$(python3 "$iri_helpers_dir/iri_helpers.py" "$service" delete-job-dir "$job_id" 2>&1) ; then
                echo "Warning: job $job_id output downloaded but remote directory deletion failed: $del_out" >&2
            fi
        fi
        # cleanup the job state file ~/.blah/iri_jobs
        if [[ -f "${jobstate_file}" ]] ; then
            rm -f "${jobstate_file}"
        fi

        # TODO: Ask Jaime how to trigger failure / job hold here
    fi

    result="[BatchJobId=\"$job_id\";JobStatus=${blahp_status};${extra_info}]"
    #if [ "$blahp_status" == "4" ] || [ "$blahp_status" == "3" ] ; then
    #    exit_code=$(map_iri_state_to_exitcode "$iri_state")
    #    result="${result}ExitCode=${exit_code};"
    #fi
    #result="${result}]"

    echo "0${result}"
done

exit 0
