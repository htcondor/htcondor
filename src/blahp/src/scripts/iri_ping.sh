#!/usr/bin/bash

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
. `dirname $0`/iri_setup.sh

echo $(/bin/date) ping "$@" >>${blah_debug_save_submit_info}/scripts.log

iri_helpers_dir=`dirname $0`

temp=${BLAHPD_IRI_SERVICE#*,}
compute_rsc=${temp%%,*}
storage_rsc=${temp#*,}

out=$(python3 "$iri_helpers_dir/iri_helpers.py" "$BLAHPD_IRI_SERVICE" resource-status "$compute_rsc")

if [ "$?" == "0" ]; then
    if [ "$out" != "up" ]; then
        echo "1 $compute_rsc is $out"
        exit 0
    fi
else
    echo "1 IRI status error: ${out}"
    exit 0
fi

out=$(python3 "$iri_helpers_dir/iri_helpers.py" "$BLAHPD_IRI_SERVICE" resource-status "$storage_rsc")

if [ "$?" == "0" ]; then
    if [ "$out" != "up" ]; then
        echo "1 $compute_rsc is $out"
        exit 0
    fi
else
    echo "1 IRI status error: ${out}"
    exit 0
fi

echo "0 No error"
exit 0
