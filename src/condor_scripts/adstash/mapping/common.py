# Copyright 2025 HTCondor Team, Computer Sciences Department,
# University of Wisconsin-Madison, WI.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# MAX_KEYWORD_LEN is the value used for ignore_above
MAX_KEYWORD_LEN = 32766

# The metadata object should always be added to the mapping last
# because adstash controls this field.
METADATA_MAPPING = {
    "metadata": {
        "properties": {
            "condor_adstash_hostname": {"type": "keyword"},
            "condor_adstash_username": {"type": "keyword"},
            "condor_adstash_runtime": {"type": "date", "format": "epoch_second||strict_date_optional_time"},
            "condor_adstash_version": {"type": "keyword"},
            "condor_adstash_platform": {"type": "keyword"},
            "condor_adstash_source": {"type": "keyword"},
            "condor_history_runtime": {"type": "date", "format": "epoch_second||strict_date_optional_time"},
            "condor_history_host_platform": {"type": "keyword"},
            "condor_history_host_version": {"type": "keyword"},
            "condor_history_host_name": {"type": "keyword"},
        },
        "type": "object",
    },
}

OTHER_MAPPING_SETTINGS = {
    "date_detection": False,
    "numeric_detection": False,
}
