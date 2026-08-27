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

import time
import json

from pathlib import Path

from adstash.utils import classad_json_serializer
from adstash.interfaces.generic import GenericInterface


class JSONFileInterface(GenericInterface):


    def __init__(self, json_dir=Path.cwd(), json_legacy=False, **kwargs):
        self.json_dir = Path(json_dir)
        self.json_legacy = json_legacy
        super().__init__(**kwargs)


    def make_bulk_body(self, docs: list, metadata=None) -> str:
        body = []
        for doc_id, doc in docs:
            doc["_id"] = doc_id
            doc["metadata"] = metadata or {}  # bolt on the metadata
            body.append(doc)

        if self.json_legacy:
            return "".join([json.dumps(doc, indent=2, sort_keys=True, default=classad_json_serializer) for doc in body])

        return json.dumps(body, indent=2, sort_keys=True, default=classad_json_serializer)


    def post_ads(self, ads, metadata=None, **kwargs):

        body = self.make_bulk_body(ads, metadata)
        json_file = self.json_dir / f"{time.time()}.json"
        with json_file.open("w") as f:
            f.write(body)

        return {"success": len(ads), "error": 0}
