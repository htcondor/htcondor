#!/usr/bin/bash
#
# Description:
#   Cancel a job via the Integrated Research Infrastructure (IRI) API,
#   to be invoked by blahpd server. Cancels one or more jobs by invoking
#   the 'cancel' subcommand of iri_helpers.py.
#   Usage:
#     iri_cancel.sh iri/<service>/<jobid> [iri/<service>/<jobid> ...]
#
# Copyright (c) HTCondor Team, Computer Sciences Department,
#   University of Wisconsin-Madison, WI. 2026.
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

bls_iri_log "$(/bin/date)" cancel "$@"

iri_helpers_dir=$(dirname "$0")

jnr=0
jc=0
for job in "$@" ; do
    jnr=$((jnr+1))
done

for job in "$@" ; do
    # Break apart blahp id "iri/<service>/<job-id>"
    # (the blahp strips off the "iri/" prefix)
    service=${job%%/*}
    job_id=${job#*/}
    bls_iri_log "  iri_helpers.py $service cancel $job_id"
    cmdout=$(python3 "$iri_helpers_dir/iri_helpers.py" "$service" cancel "$job_id" 2>&1)
    retcode=$?
    # TODO handle errors, including job-not-found
    # If the job is already completed or no longer in the queue,
    # treat it as successfully deleted.
    if [ "$retcode" == "0" ] ; then
        if [ "$jnr" == "1" ]; then
            echo " 0 No\\ error"
        else
            echo .$jc" 0 No\\ error"
        fi
    else
        escaped_cmdout=${cmdout// /\\ }
        if [ "$jnr" == "1" ]; then
            echo " $retcode $escaped_cmdout"
        else
            echo .$jc" $retcode $escaped_cmdout"
        fi
    fi
    jc=$((jc+1))
done
