#!/bin/bash

#  File:     condor_hold.sh
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

# The first and only argument is a JobId whose format is: Id/Queue/Pool

id=${1%%/*} # Id, everything before the first / in Id/Queue/Pool
queue_pool=${1#*/} # Queue/Pool, everything after the first /  in Id/Queue/Pool
queue=${queue_pool%/*} # Queue, everything before the first / in Queue/Pool
pool=${queue_pool#*/} # Pool, everything after the first / in Queue/Pool

if [ -z "$queue" ]; then
    target=""
else
    if [ -z "$pool" ]; then
	target="-name $queue"
    else
	target="-pool $pool -name $queue"
    fi
fi

${condor_binpath}condor_hold $target $id >&/dev/null

if [ "$?" == "0" ]; then
    echo " 0 No\\ error"
    exit 0
else
    echo " 1 Error"
    exit 1
fi
