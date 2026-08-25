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

from adstash.ad_converters.generic import GenericClassAdConverter
from adstash.mapping.job import IGNORE_ATTRS

import classad2 as classad


# Attributes to be used in all projections to condor_history
REQUIRED_ATTRS = {
    "ClusterId",
    "CompletionDate",
    "EnteredCurrentStatus",
    "GlobalJobId",
    "JobStatus",
    "JobUniverse",
    "LastRemoteHost",
    "MyType",
    "ProcId",
    "RemoteHost",
}

# Attributes (in order) which should be used as a timestamp
TIMESTAMP_ATTRS = [
    "CompletionDate",
    "EnteredCurrentStatus",
]

# Attributes (in order) which should be used to unique identify a job/epoch
DOC_ID_ATTRS = [
    "GlobalJobId",
    "RecordTime",  # This is a derived timestamp that will always be available
]

STATUS = {
    0: "Unexpanded",
    1: "Idle",
    2: "Running",
    3: "Removed",
    4: "Completed",
    5: "Held",
    6: "Error",
}

UNIVERSE = {
    1: "Standard",
    2: "Pipe",
    3: "Linda",
    4: "PVM",
    5: "Vanilla",
    6: "PVMD",
    7: "Scheduler",
    8: "MPI",
    9: "Grid",
    10: "Java",
    11: "Parallel",
    12: "Local",
}


class JobClassAdConverter(GenericClassAdConverter):

    # Override the default attr sets/lists
    def __init__(
            self,
            ignore_attrs=IGNORE_ATTRS,
            required_attrs=REQUIRED_ATTRS,
            timestamp_fields=TIMESTAMP_ATTRS,
            doc_id_fields=DOC_ID_ATTRS,
            **kwargs,
        ):
        super().__init__(
            ignore_attrs=ignore_attrs,
            required_attrs=required_attrs,
            timestamp_fields=timestamp_fields,
            doc_id_fields=doc_id_fields,
            **kwargs,
            )

    def add_additional_fields(self, doc: dict, ad: classad.ClassAd):
        doc["ScheddName"] = ad.get("GlobalJobId", "UNKNOWN").split("#")[0]
        doc["StartdSlot"] = ad.get("RemoteHost", ad.get("LastRemoteHost", "UNKNOWN@UNKNOWN")).split("@")[0]
        doc["StartdName"] = ad.get("RemoteHost", ad.get("LastRemoteHost", "UNKNOWN@UNKNOWN")).split("@")[-1]
        doc["Status"] = STATUS.get(ad.get("JobStatus"), "Unknown")
        doc["Universe"] = UNIVERSE.get(ad.get("JobUniverse"), "Unknown")
