#!/bin/bash
#
# File:     sfapi_cancel.sh
# Author:   Karan Vahi (vahi@isi.edu)
#
# Description:
#   Cancel script for jobs submitted to Perlmutter via the NERSC SFAPI,
#   to be invoked by blahpd server. Cancels one or more jobs by invoking
#   the 'cancel' subcommand of sfapi_helpers.py.
#   Usage:
#     sfapi_cancel.sh sfapi/<date>/<jobid> [sfapi/<date>/<jobid> ...]
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

jnr=0
jc=0
for job in $@ ; do
    jnr=$(($jnr+1))
done

for job in $@ ; do
    cmdout=$(python3 "$sfapi_helpers_dir/sfapi_helpers.py" cancel "$job" 2>&1)
    retcode=$?
    # If the job is already completed or no longer in the queue,
    # treat it as successfully deleted.
    if echo "$cmdout" | grep -q 'sfapi_client.exceptions.SfApiError: Job not found:' ; then
        retcode=0
    fi
    if [ "$retcode" == "0" ] ; then
        if [ "$jnr" == "1" ]; then
            echo " 0 No\\ error"
        else
            echo .$jc" 0 No\\ error"
        fi
    else
        escaped_cmdout=`echo $cmdout | sed "s/ /\\\\\ /g"`
        if [ "$jnr" == "1" ]; then
            echo " $retcode $escaped_cmdout"
        else
            echo .$jc" $retcode $escaped_cmdout"
        fi
    fi
    jc=$(($jc+1))
done
