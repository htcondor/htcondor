#!/bin/bash

# File:     flux_resume.sh
# Author:   Ian Lumsden (ilumsden@vols.utk.edu)
# Author:   Michela Taufer (mtaufer@utk.edu)
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

if [ -z "$flux_binpath" ] ; then
  flux_binpath=/usr/bin
fi

. `dirname $0`/flux_utils.sh

flux_utils_check_or_get_requeued_job "$1" "requested" "cluster_name"

${flux_binpath}/flux job urgency $requested DEFAULT >&/dev/null

if [ "$?" == "0" ]; then
  echo " 0 No\\ error"
  exit 0
else
  echo " 1 Error"
  exit 1
fi

