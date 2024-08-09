# Copyright 2022 HTCondor Team, Computer Sciences Department,
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


def schedd_history_source():
    from adstash.ad_sources.schedd_history import ScheddHistorySource
    return ScheddHistorySource
def startd_history_source():
    from adstash.ad_sources.startd_history import StartdHistorySource
    return StartdHistorySource
def schedd_job_epoch_history_source():
    from adstash.ad_sources.schedd_job_epoch_history import ScheddJobEpochHistorySource
    return ScheddJobEpochHistorySource
def ad_file_source():
    from adstash.ad_sources.ad_file import FileAdSource
    return FileAdSource


ADSTASH_AD_SOURCE_REGISTRY = {
    "schedd_history": schedd_history_source,
    "startd_history": startd_history_source,
    "schedd_job_epoch_history": schedd_job_epoch_history_source,
    "ad_file": ad_file_source,
}
ADSTASH_AD_SOURCES = list(ADSTASH_AD_SOURCE_REGISTRY.keys())
