#!/bin/bash

#  File:     pbs_hold.sh
#
#  Author:   David Rebatto
#  e-mail:   David.Rebatto@mi.infn.it
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

requested=`echo $1 | sed 's/^.*\///'`
requestedshort=`expr match "$requested" '\([0-9]*\)'`
result=`${pbs_binpath}/qstat | awk -v jobid="$requestedshort" '
$0 ~ jobid {
	print $5
}
'`
#currently only holding idle or waiting jobs is supported
if [ "$2" ==  "1" ] ; then
	${pbs_binpath}/qhold $requested
else
	if [ "$result" == "W" ] ; then
		${pbs_binpath}/qhold $requested
	else
		echo "unsupported for this job status" >&2
		exit 1
	fi
fi
