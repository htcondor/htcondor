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

import json

from pathlib import Path

from adstash.interfaces.json_file import JSONFileInterface as Interface

class JSONFileInterface(Interface):
    """Append ClassAds to a single file where each ad is a single json line.

    This interface is almost identical to json_file.JSONFileInterface,
    but appends to the same log file instead of creating a new one each time.
    Each 'ad' is written without indention (not 'pretty').
    
    Most log aggregators watch a log file and expect logs to be separated by a new-line character.
    Each additional line is then processed as an incoming log to be sent to a database or log server.

    The log file should be rotated by an external tool like: 'logrotate'

    use the following configuration macro:
    ADSTASH_INTERFACE = jsonlinefile
    """

    def __init__(self, json_dir=Path.cwd(), log_mappings=True, **kwargs):
        super().__init__(json_dir, log_mappings, **kwargs)

    def post_ads(self, ads, metadata={}, **kwargs):
        body = self.make_body(ads, metadata)
        if len(body) > 0:
            self.write_mappings(self.log_mappings, self.json_dir, **kwargs)

        json_file = self.json_dir / "adstash_line_file.json"
        # open the file in 'append' mode
        with json_file.open("a") as f:
            for ad in body:
                # write but do not indent
                json.dump(ad, f, sort_keys=True)
                # append newline
                f.write("\n")

        return {"success": len(body), "error": 0}
