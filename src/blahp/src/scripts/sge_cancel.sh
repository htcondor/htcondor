#!/bin/bash

# File:     sge_cancel.sh
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

if [ -z "$sge_rootpath" ]; then sge_rootpath="/usr/local/sge/pro"; fi
if [ -r "$sge_rootpath/${sge_cellname:-default}/common/settings.sh" ]
then
  . $sge_rootpath/${sge_cellname:-default}/common/settings.sh
fi

jnr=0
jc=0
for job in  $@ ; do
        jnr=$(($jnr+1))
done
for  job in  $@ ; do
        requested=`echo $job | sed 's/^.*\///'`
	requestedshort=`expr match "$requested" '\([0-9]*\)'`

        qdel $requestedshort >/dev/null 2>&1
        if [ "$?" == "0" ] ; then
                if [ "$jnr" == "1" ]; then
                        echo " 0 No\\ error"
                else
                        echo .$jc" 0 No\\ error"
                fi
        else
                if [ "$jnr" == "1" ]; then
                        echo " 1 Error"
                else
                        echo .$jc" 1 Error"
                fi
        fi
        jc=$(($jc+1))
done

