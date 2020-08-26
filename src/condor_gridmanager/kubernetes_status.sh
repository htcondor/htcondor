#!/bin/bash

#  File:     kubernetes_status.sh
#
#  Author:   Francesco Prelz
#  e-mail:   Francesco.Prelz@mi.infn.it
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
echo status $* >>/tmp/k8s.log


. `dirname $0`/blah_load_config.sh


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

# The job's format is: kubernetes/Id
id=${1#k*/} # Strip off the leading "kubernetes/"

cmdout=`kubectl get pod $id 2>&1`
retcode=$?
# If the job is no longer in the queue, treat it as completed.
if echo "$cmdout" | grep -q 'NotFound' ; then
    echo "0[BatchjobId=\"${id}\";JobStatus=4;ExitBySignal=false;ExitCode=0]"
    exit 0
fi

if [ "$retcode" != "0" ] ; then
    echo " 1 Error"
    exit 1
fi
if echo "$cmdout" | grep -q 'Completed' ; then
    # TODO handle failure to delete
    kubectl delete pods/$id >/dev/null 2>&1
    echo "0[BatchjobId=\"${id}\";JobStatus=4;ExitBySignal=false;ExitCode=0]"
    exit 0
else
    # TODO use Age column for RemoteWallClockTime
    echo "0[BatchjobId=\"${id}\";JobStatus=2]"
    exit 0
fi
