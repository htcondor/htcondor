#!/bin/bash

# File:     slurm_cancel.sh
# Author:   Jaime Frey (jfrey@cs.wisc.edu)
# Based on code by David Rebatto (david.rebatto@mi.infn.it)
# 
# Copyright (c) Members of the EGEE Collaboration. 2004. 
# Copyright (c) HTCondor Team, Computer Sciences Department,
#   University of Wisconsin-Madison, WI. 2015.
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

if [ -z "$slurm_binpath" ] ; then
  slurm_binpath=/usr/bin
fi

jnr=0
jc=0
for job in  $@ ; do
        jnr=$(($jnr+1))
done
for  job in  $@ ; do
        requested=`echo $job | sed 's/^.*\///'`
        cluster_name=`echo $requested | cut -s -f2 -d@`
        if [ "$cluster_name" != "" ] ; then
            requested=`echo $requested | cut -f1 -d@`
            cluster_arg="-M $cluster_name"
        fi
        cmdout=`${slurm_binpath}/scancel $cluster_arg $requested 2>&1`
        retcode=$?
        # If the job is already completed or no longer in the queue,
        # treat it as successfully deleted.
        if echo "$cmdout" | grep -q 'Invalid job id specified' ; then
                retcode=0
        fi
        if [ "$retcode" == "0" ] ; then
                if [ "$jnr" == "1" ]; then
                        echo " 0 No\\ error"
                else
                        echo .$jc" 0 No\\ error"
                fi
        else
                escaped_cmdout=`echo $cmdout|sed "s/ /\\\\\ /g"`
                if [ "$jnr" == "1" ]; then
                        echo " $retcode $escaped_cmdout"
                else
                        echo .$jc" $retcode $escaped_cmdout"
                fi
        fi
        jc=$(($jc+1))
done

