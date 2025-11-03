#!/bin/bash

# File:     slurm_resume.sh
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

requested=`echo $1 | sed 's/^.*\///'`

cluster_name=`echo $requested | cut -s -f2 -d@`
if [ "$cluster_name" != "" ] ; then
  requested=`echo $requested | cut -f1 -d@`
  cluster_arg="-M $cluster_name"
fi

${slurm_binpath}/scontrol $cluster_name release $requested >&/dev/null

if [ "$?" == "0" ]; then
  echo " 0 No\\ error"
  exit 0
else
  echo " 1 Error"
  exit 1
fi

