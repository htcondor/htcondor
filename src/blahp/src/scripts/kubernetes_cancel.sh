#!/bin/bash

#  File:     kubernetes_cancel.sh
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
echo cancel $* >>/tmp/k8s.log

. `dirname $0`/blah_load_config.sh

# The first and only argument is a JobId whose format is: Id

id=${1} # Id, which is just the name of the pod

# Assume kubectl is in the PATH
cmdout=`kubectl delete pods/$id 2>&1`
retcode=$?

# If the job is no longer in the queue, treat it as successfully removed.
if echo "$cmdout" | grep -q 'NotFound' ; then
    retcode=0
fi

if [ "$?" == "0" ]; then
    echo " 0 No\\ error"
    exit 0
else
    echo " 1 Error"
    exit 1
fi

