# Copyright 2026 HTCondor Team, Computer Sciences Department,
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

import json

from adstash.utils import classad_json_serializer
from adstash.interfaces.json_file import JSONFileInterface


class NDJSONFileInterface(JSONFileInterface):
    """Append ClassAds to a single file where each ad is a single json line.

    This interface is almost identical to json_file.JSONFileInterface,
    but appends to the same log file instead of creating a new one each time.
    Each 'ad' is written without indention (not 'pretty').

    Most log aggregators watch a log file and expect logs to be separated by a new-line character.
    Each additional line is then processed as an incoming log to be sent to a database or log server.

    The log file should be rotated by an external tool like: 'logrotate'

    use the following configuration macro:
    ADSTASH_INTERFACE = ndjsonfile
    """

    def make_bulk_body(self, docs: list, metadata=None) -> str:
        body = []
        for doc_id, doc in docs:
            doc["_id"] = doc_id
            doc["metadata"] = metadata or {}  # bolt on the metadata
            body.append(doc)

        # dump each ad as a separate line, without indentation
        return "\n".join(
            [json.dumps(doc, indent=None, sort_keys=True, default=classad_json_serializer) for doc in body]
        )

    def post_ads(self, ads, metadata=None, **kwargs):
        body = self.make_bulk_body(ads, metadata)
        json_file = self.json_dir / "adstash.jsonl"
        # open the file in 'append' mode
        with json_file.open("a") as f:
            f.write(body)
            # append newline on the last line
            f.write("\n")

        return {"success": len(ads), "error": 0}

