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

from adstash.ad_converters.job import JobClassAdConverter, REQUIRED_ATTRS


# Attributes to be used in all projections to condor_history
REQUIRED_ATTRS = REQUIRED_ATTRS | {"EpochWriteDate", "EpochAdType"}

# Attributes (in order) which should be used as a timestamp
TIMESTAMP_ATTRS = [
    "EpochWriteDate",
]

# Attributes (in order) which should be used to unique identify a job/epoch
DOC_ID_ATTRS = [
    "GlobalJobId",
    "NumShadowStarts",
    "RecordTime",
]


class JobEpochClassAdConverter(JobClassAdConverter):

    # Override the default attr sets/lists
    def __init__(
            self,
            required_attrs=REQUIRED_ATTRS,
            timestamp_fields=TIMESTAMP_ATTRS,
            doc_id_fields=DOC_ID_ATTRS,
            **kwargs,
            ):
        super().__init__(
            required_attrs=required_attrs,
            timestamp_fields=timestamp_fields,
            doc_id_fields=doc_id_fields,
            **kwargs
            )
