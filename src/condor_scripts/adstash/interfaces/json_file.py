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
import logging

from pathlib import Path

from adstash.interfaces.null import NullInterface
import adstash.convert as convert

class JSONFileInterface(NullInterface):


    def __init__(self, json_dir=Path.cwd(), log_mappings=True, **kwargs):
        self.json_dir = Path(json_dir)
        self.log_mappings = log_mappings


    def write_mappings(self, log_mappings, json_dir, **kwargs):
        if not log_mappings:
            return

        mappings = self.make_mappings(**kwargs)
        settings = self.make_settings(**kwargs)
        body = {"mappings": mappings, "settings": {"index": settings}}

        mappings_file = Path(json_dir) / "condor_adstash_jsonfile_last_mappings.json"
        logging.debug(f"Writing mappings to {mappings_file}.")
        json.dump(body, open(mappings_file, "w"), indent=2, sort_keys=True)


    def make_body(self, ads, metadata={}):
        body = []
        for id_, ad in ads:
            if metadata:
                ad.setdefault("metadata", {}).update(metadata)

            ad.update({"_id": id_})
            body.append(ad)

        return body


    # Not great that this is copied from ElasticsearchInterface, but
    # this prevents needing to load the elasticsearch library to use
    # this interface.
    def make_mappings(self, ad_source=None, **kwargs):
        properties = {}

        properties["metadata"] = {
            "properties": {
                "adstash_runtime": {"type": "date", "format": "epoch_second"},
                "condor_history_runtime": {"type": "date", "format": "epoch_second"},
            },
            "type": "nested",
            "dynamic": True,
        }

        if ad_source is None:

            for name in convert.TEXT_ATTRS:
                properties[name] = {"type": "text"}
            for name in convert.INDEXED_KEYWORD_ATTRS:
                properties[name] = {"type": "keyword"}
            for name in convert.NOINDEX_KEYWORD_ATTRS:
                properties[name] = {"type": "keyword", "index": "false"}
            for name in convert.FLOAT_ATTRS:
                properties[name] = {"type": "double"}
            for name in convert.INT_ATTRS:
                properties[name] = {"type": "long"}
            for name in convert.DATE_ATTRS:
                properties[name] = {"type": "date", "format": "epoch_second"}
            for name in convert.BOOL_ATTRS:
                properties[name] = {"type": "boolean"}
            for name in convert.NESTED_ATTRS:
                properties[name] = {"type": "nested", "dynamic": True}

            dynamic_templates = [
                {
                    "raw_expressions": {  # Attrs ending in "_EXPR" are generated during
                        "match": "*_EXPR",  # ad conversion for expressions that cannot be evaluated
                        "mapping": {"type": "keyword", "index": "false", "ignore_above": 256},
                    }
                },
                {
                    "date_attrs": {  # Attrs ending in "Date" are usually timestamps
                        "match": "*Date",
                        "mapping": {"type": "date", "format": "epoch_second"},
                    }
                },
                {
                    "provisioned_attrs": {  # Attrs ending in "Provisioned" are
                        "match": "*Provisioned",  # resource numbers
                        "mapping": {"type": "long"},
                    }
                },
                {
                    "resource_request_attrs": {  # Attrs starting with "Request" are
                        "match_pattern": "regex",  # usually resource numbers
                        "match": "^Request[A-Z].*$",
                        "mapping": {"type": "long"},
                    }
                },
                {
                    "target_boolean_attrs": {  # Attrs starting with "Want", "Has", or
                        "match_pattern": "regex",  # "Is" are usually boolean checks on the
                        "match": "^(Want|Has|Is)[A-Z_].*$",  # target machine
                        "mapping": {"type": "boolean"},
                    }
                },
                {
                    "strings_as_keywords": {  # Store unknown strings as keywords
                        "match_mapping_type": "string",
                        "mapping": {"type": "keyword", "norms": "false", "ignore_above": 256},
                    }
                },
            ]

        else:

            for name in ad_source.text_attrs:
                properties[name] = {"type": "text"}
            for name in ad_source.indexed_keyword_attrs:
                properties[name] = {"type": "keyword"}
            for name in ad_source.noindex_keyword_attrs:
                properties[name] = {"type": "keyword", "index": "false"}
            for name in ad_source.float_attrs:
                properties[name] = {"type": "double"}
            for name in ad_source.int_attrs:
                properties[name] = {"type": "long"}
            for name in ad_source.date_attrs:
                properties[name] = {"type": "date", "format": "epoch_second"}
            for name in ad_source.bool_attrs:
                properties[name] = {"type": "boolean"}
            for name in ad_source.nested_attrs:
                properties[name] = {"type": "nested", "dynamic": True}
            dynamic_templates = ad_source.dynamic_templates

        mappings = {
            "dynamic_templates": dynamic_templates,
            "properties": properties,
            "date_detection": False,
            "numeric_detection": False,
        }

        return mappings


    # Not great that this is copied from ElasticsearchInterface, but
    # this prevents needing to load the elasticsearch library to use
    # this interface.
    def make_settings(self, **kwargs):
        settings = {
            "analysis": {
                "analyzer": {
                    "analyzer_keyword": {"tokenizer": "keyword", "filter": "lowercase"}
                }
            },
            "mapping.total_fields.limit": 5000,
        }
        return settings


    def post_ads(self, ads, metadata={}, **kwargs):
        body = self.make_body(ads, metadata)
        if len(body) > 0:
            self.write_mappings(self.log_mappings, self.json_dir, **kwargs)

        json_file = self.json_dir / f"{time.time()}.json"
        with json_file.open("w") as f:
            for ad in body:
                json.dump(ad, f, indent=2, sort_keys=True)

        return {"success": len(body), "error": 0}
