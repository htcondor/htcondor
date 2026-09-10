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

from adstash.mapping.common import MAX_KEYWORD_LEN

from adstash.mapping.job import (
    INDEXED_KEYWORD_ATTRS,
    NON_INDEXED_KEYWORD_ATTRS,
    INDEXED_TEXT_ATTRS,
    NON_INDEXED_TEXT_ATTRS,
    FLOAT_ATTRS,
    INT_ATTRS,
    DATE_ATTRS,
    BOOL_ATTRS,
    OBJECT_ATTRS,
    NESTED_ATTRS,
    IGNORE_ATTRS,
    DYNAMIC_TEMPLATES,
    METADATA_MAPPING,
    OTHER_MAPPING_SETTINGS,
)

INDEXED_KEYWORD_ATTRS = INDEXED_KEYWORD_ATTRS.copy()
# NON_INDEXED_KEYWORD_ATTRS = NON_INDEXED_KEYWORD_ATTRS.copy()
# INDEXED_TEXT_ATTRS = INDEXED_TEXT_ATTRS.copy()
# NON_INDEXED_TEXT_ATTRS = NON_INDEXED_TEXT_ATTRS.copy()
# FLOAT_ATTRS = FLOAT_ATTRS.copy()
# INT_ATTRS = INT_ATTRS.copy()
DATE_ATTRS = DATE_ATTRS.copy()
# BOOL_ATTRS = BOOL_ATTRS.copy()
# OBJECT_ATTRS = OBJECT_ATTRS.copy()
# NESTED_ATTRS = NESTED_ATTRS.copy()
# IGNORE_ATTRS = IGNORE_ATTRS.copy()
# DYNAMIC_TEMPLATES = DYNAMIC_TEMPLATES.copy()

# Job epoch ads are pretty much the same as job ads
# when it comes to mapping except for the addition
# of EpochAdType and EpochWriteDate
INDEXED_KEYWORD_ATTRS.add("EpochAdType")
DATE_ATTRS.add("EpochWriteDate")
