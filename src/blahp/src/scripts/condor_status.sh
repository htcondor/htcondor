#!/bin/bash

#  File:     condor_status.sh
#
#  Author:   Matt Farrellee (Condor) - received on March 28, 2007
#
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

if [ "x$job_registry" != "x" ] ; then
   ${blah_sbin_directory}/blah_job_registry_lkup $@
   exit 0
fi

FORMAT='-format "%d" ClusterId -format "," ALWAYS -format "%d" JobStatus -format "," ALWAYS -format "%f" RemoteSysCpu -format "," ALWAYS -format "%f" RemoteUserCpu -format "," ALWAYS -format "%f" BytesSent -format "," ALWAYS -format "%f" BytesRecvd -format "," ALWAYS -format "%f" RemoteWallClockTime -format "," ALWAYS -format "%d" ExitBySignal -format "," ALWAYS -format "%d" ExitCode -format "%d" ExitSignal -format "\n" ALWAYS'

# The "main" for this script is way at the bottom of the file.

REFRESH_RATE=30 # seconds
CACHE_PREFIX="cache"

# Identify the current cache, if there is none an empty string is
# returned.
function identify_cache {
    local queue=$1
    local pool=$2

    echo $GAHP_TEMP/$CACHE_PREFIX.$queue.$pool
}

# This function waits on the given queue's cache. It also returns the
# name of the cache it believes to be the most recently update. Cache
# readers should be aware that there is a race condition here if the
# cache is being updated after this function returns and the caller
# tries to read from the cache. There is no read lock on the
# cache. This error is transient so a retry can be used.
function wait_on_cache {
    local queue=$1
    local pool=$2

    local cache=$(identify_cache $queue $pool)
    while [ -f "$cache.barrier" ]; do
	sleep 1
    done

    echo $cache
}

# Try and update the cache. If condor_q fails 2 is returned and likely
# is not recoverable. Otherwise the status (0|1) from writing the new
# cache is returned. 0 is of course success and 1 means that the cache
# already exists, i.e. someone else wrote it already.
function update_cache {
    local queue=$1
    local pool=$2

    local cache=$(identify_cache $queue $pool)

    # This line is critical for synchronization, it makes sure
    # concurrently running scripts do not overwrite the same cache
    # file.
    set -o noclobber

    echo "$(date +%s)" 2>>/dev/null > $cache.barrier
    if [ "$?" == "1" ]; then
	# Someone else is updating the cache. We want to wait for it
	# to finish then return.
	wait_on_cache $queue $pool >/dev/null # Throw away result, just wait

	return 1
    fi

    if [ -z "$queue" ]; then
	local target=""
    else
	if [ -z "$pool" ]; then
	    local target="-name $queue"
	else
	    local target="-pool $pool -name $queue"
	fi
    fi

    local data=$(echo $FORMAT | xargs $condor_binpath/condor_q $target)

    if [ "$?" == "0" ]; then
	set +o noclobber
	printf "%s\n%s\n" $(date +%s) "$data" > $cache
	rm -f $cache.barrier

	return $code
    else
	rm -f $cache.barrier # Update not attempted

	# 2 represents an error in condor_q
	return 2
    fi
}

# Search the cache with grep, if no line is found update the cache
# and try again.
function search {
    local key=$1
    local queue=$2
    local pool=$3

    if [ ! -s "$(identify_cache $queue $pool)" ]; then
	# No cache found, make one
	#echo "$$ No cache" >>log
	update_cache $queue $pool
	if [ "$?" == "2" ]; then
	    # We can't recover from a condor_q failure, the other
	    # error just means the cache has been updated by someone
	    # else.
	    return 1
	fi
    else
	local seconds=$(date +%s)
	local old_seconds=$(head -n 1 $(wait_on_cache $queue $pool) 2>/dev/null) # RACE CONDITION, no read lock, must validate old_seconds 
	if [ ! -z "$old_seconds" -a $seconds -gt $((old_seconds + REFRESH_RATE)) ]; then
	    # Cache is out of date, refresh it
	    #echo "$$ Cache out of date" >>log
	    update_cache $queue $pool
	    if [ "$?" == "2" ]; then

		# error just means the cache has been updated by someone
		# else.
		return 1
	    fi
	fi
    fi

    local line=`grep "^$key[^0-9]" $(wait_on_cache $queue $pool) 2>/dev/null` # RACE CONDITION, no read lock

    if [ ! -z "$line" ]; then
	echo $line
	return 0
    fi

    # Line not found, update the cache and try again.
    #echo "$$ No line, update cache" >>log
    update_cache $queue $pool
    if [ "$?" == "2" ]; then
        # We can't recover from a condor_q failure, the other
        # error just means the cache has been updated by someone
        # else.
	return 1
    fi

    local line=`grep "^$key[^0-9]" $(wait_on_cache $queue $pool)` # RACE CONDITION, no read lock

    # NOTE: Doing the search twice helps to mitigate the race
    # condition. It would have to occur twice for no result to be
    # returned when one exists. The caller should do a condor_q to be
    # sure no result exists.

    if [ ! -z "$line" ]; then
	echo $line
	return 0
    fi

    echo ""
    return 1
}

# Given a cache line generate an ad.
function make_ad {
    local job=$1
    local line=$2

    local cluster=$(echo $line | awk -F ',' '{print $1}')
    local status=$(echo $line | awk -F ',' '{print $2}')
    local remote_sys_cpu=$(echo $line | awk -F ',' '{print $3}')
    local remote_user_cpu=$(echo $line | awk -F ',' '{print $4}')
    local bytes_sent=$(echo $line | awk -F ',' '{print $5}')
    local bytes_recvd=$(echo $line | awk -F ',' '{print $6}')
    local remote_wall_clock_time=$(echo $line | awk -F ',' '{print $7}')
    local exit_by_signal=$(echo $line | awk -F ',' '{print $8}')
    local code_or_signal=$(echo $line | awk -F ',' '{print $9}')

    echo -n "[BatchjobId=\"$job\";JobStatus=$status;RemoteSysCpu=${remote_sys_cpu:-0};RemoteUserCpu=${remote_user_cpu:-0};BytesSent=${bytes_sent:-0};BytesRecvd=${bytes_recvd:-0};RemoteWallClockTime=${remote_wall_clock_time:-0};"
    if [ "$status" == "4" ] ; then
	if [ "$exit_by_signal" == "0" ] ; then
	    echo -n "ExitBySignal=FALSE;ExitCode=${code_or_signal:-0}"
	else
	    echo -n "ExitBySignal=TRUE;ExitSignal=${code_of_signal:-0}"
	fi
    fi
    echo "]"
}

function at_exit {
    local cache=$(identify_cache $queue $pool)
    rm -f "$cache.barrier"
}
trap at_exit EXIT

### main

while getopts "wn" arg 
do
    case "$arg" in
    w) ;;
    n) ;;
    -) break ;;
    ?) echo "Usage: $0 [-w] [-n]"
       exit 1 ;;
    esac
done

shift `expr $OPTIND - 1`

for job in $* ; do
# The job's format is: condor/Id/Queue/Pool
    job=${job#con*/} # Strip off the leading "con(dor)/"
    id=${job%%/*} # Id, everything before the first / in Id/Queue/Pool
    queue_pool=${job#*/} # Queue/Pool, everything after the first /  in Id/Queue/Pool
    queue=${queue_pool%/*} # Queue, everything before the first / in Queue/Pool
    pool=${queue_pool#*/} # Pool, everything after the first / in Queue/Pool

    line=$(search $id $queue $pool)
    if  [ ! -z "$line" ] ; then
	echo "0$(make_ad $job "$line")"
	exit 0
    fi

    if [ -z "$queue" ]; then
	target=""
    else
	if [ -z "$pool" ]; then
	    target="-name $queue"
	else
	    target="-pool $pool -name $queue"
	fi
    fi

    # Caching of condor_q output doesn't appear to work properly in
    # HTCondor builds of the blahp. So do an explicit condor_q for
    # this job before trying condor_history, which can take a long time.
    line=$(echo $FORMAT | xargs $condor_binpath/condor_q $target $id)
    if  [ -n "$line" ] ; then
	echo "0$(make_ad $job "$line")"
	exit 0
    fi

    ### WARNING: This is troubling because the remote history file
    ### might just happen to be in the same place as a local history
    ### file, in which case condor_history is going to be looking at
    ### the history of an unexpected queue.

    # We can possibly get the location of the history file and check it.
    # NOTE: In Condor 7.7.6-7.8.1, the -f option to condor_history was
    #   broken. To work around that, we set HISTORY via the environment
    #   instead of using -f.
    history_file=$($condor_binpath/condor_config_val $target -schedd history)
    if [ "$?" == "0" ]; then
	line=$(echo $FORMAT | _condor_HISTORY="$history_file" xargs $condor_binpath/condor_history -f $history_file -backwards -match 1 $id)
	if  [ ! -z "$line" ] ; then
	    echo "0$(make_ad $job "$line")"
	    exit 0
	fi
    fi

    # If we still do not have a result it is possible that a race
    # condition masked the status, in which case we want to directly
    # query the Schedd to make absolutely sure there is no status to
    # be found.
    line=$(echo $FORMAT | xargs $condor_binpath/condor_q $target $id)
    if  [ -z "$line" ] ; then
	echo " 1 Status\\ not\\ found"
	exit 1
    else
	echo "0$(make_ad $job "$line")"
	exit 0
    fi

done
