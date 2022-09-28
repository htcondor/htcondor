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


def null_interface():
    from adstash.interfaces.null import NullInterface
    return NullInterface
def elasticsearch_interface():
    from adstash.interfaces.elasticsearch import ElasticsearchInterface
    return ElasticsearchInterface
def opensearch_interface():
    from adstash.interfaces.opensearch import OpenSearchInterface
def json_file_interface():
    from adstash.interfaces.json_file import JSONFileInterface
    return JSONFileInterface


ADSTASH_INTERFACE_REGISTRY = {
    "null": {"class": null_interface, "type": None},
    "elasticsearch": {"class": elasticsearch_interface, "type": "se"},
    "opensearch": {"class": opensearch_interface, "type": "se"},
    "jsonfile": {"class": json_file_interface, "type": "jsonfile"},
}
ADSTASH_INTERFACES = list(ADSTASH_INTERFACE_REGISTRY.keys())
