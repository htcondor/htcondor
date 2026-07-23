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

import logging
import json
import sys

from pathlib import Path

from adstash.mapping.functions import count_total_fields

DEFAULT_INITIAL_SETTINGS = {
    "index": {
        "mapping": {
            "ignore_malformed": True,  # https://www.elastic.co/guide/en/elasticsearch/reference/7.17/ignore-malformed.html#ignore-malformed-setting
        },
        "refresh_interval": "60s",  # https://www.elastic.co/guide/en/elasticsearch/reference/7.17/tune-for-indexing-speed.html#_unset_or_increase_the_refresh_interval
        "write": {
            "wait_for_active_shards": "all",  # block writes unless all shard copies are available
        },
    }
}

DEFAULT_ILM_POLICY = {
    "policy": {
        "phases": {
            "hot": {
                "min_age": "0ms",
                "actions": {
                    "rollover": {
                        "max_primary_shard_size": "50gb",  # 50gb segments
                        "max_age": "120d"  # rollover approx. quarterly
                    },
                    "set_priority": {
                        "priority": 100
                    }
                }
            },
            "warm": {
                "min_age": "120d",  # approx. one quarter after first doc ingestion
                "actions": {
                    "forcemerge": {
                        "max_num_segments": 1  # merge down to single segment
                    },
                    "set_priority": {
                        "priority": 50
                    }
                }
            },
            "cold": {
                "min_age": "400d",  # approx. one year after first doc ingestion (extra buffer for annual reporting)
                "actions": {
                    "set_priority": {
                        "priority": 0
                    }
                }
            }
        }
    }
}


def calculate_field_limit(mappings, previous_limit=0):
    """Return the total_fields.limit to use for the given mappings (2x field count, at least previous_limit)."""
    return max(2 * count_total_fields(mappings), int(previous_limit))


class SearchEngineSettings():

    def __init__(self, index_name, mappings, custom_settings=None, existing_settings=None):
        self.alias = index_name
        self.mappings = mappings
        self.existing_settings = existing_settings or DEFAULT_INITIAL_SETTINGS
        self.settings = self.flatten_settings(self.existing_settings)
        self.update_settings = self.flatten_settings(custom_settings or {})
        self._calculate_update_settings_fields_limit()
        self.index_template_name = f"{index_name}-template"
        self.index_definition = {
            "settings": self.settings,
            "mappings": self.mappings,
        }

    def flatten_settings(self, settings=None, parent=""):
        settings = settings or {}
        flattened_settings = {}
        for k, v in settings.items():
            if parent:
                k = f"{parent}.{k}"
            if isinstance(v, dict):
                flattened_settings.update(self.flatten_settings(v, k))
                continue
            flattened_settings[k] = v
        return flattened_settings

    def merge_settings(self, *settings_list):
        merged_settings = {}
        for settings in settings_list:
            merged_settings.update(self.flatten_settings(settings))
        return merged_settings

    def update_mappings(self, mappings):
        self.mappings = mappings
        self._calculate_update_settings_fields_limit()
        self.settings = self.merge_settings(self.existing_settings, self.update_settings)

    def get_index_template(self):
        index_template = {
            "index_patterns": [f"{self.alias}-*"],
            "template": self.index_definition,
        }
        return index_template

    # The number of fields that end up getting added over time is sadly quite large.
    # Consider that any custom attribute, unless it is somehow added to the ignored attr list
    # will cause a new field to be mapped. So in order to make sure we don't drop any docs,
    # this the field limit needs to be upped occasionally.
    def _calculate_update_settings_fields_limit(self):
        previous_limit = int(self.settings.get("index.mapping.total_fields.limit", 0))
        self.update_settings["index.mapping.total_fields.limit"] = calculate_field_limit(self.mappings, previous_limit)
        # Using limit = 5000 as an arbitrary point to start warning about performance degradation
        if self.update_settings["index.mapping.total_fields.limit"] > 5000 and self.update_settings["index.mapping.total_fields.limit"] > previous_limit:
            logging.warning(f"Large index.mapping.total_fields.limit: {self.update_settings['index.mapping.total_fields.limit']}")
            logging.warning("Fields accumulate over time due to new attributes (custom or first-class).")
            logging.warning("You may experience degraded performance when the limit gets large, see e.g.:")
            logging.warning("https://www.elastic.co/docs/reference/elasticsearch/index-settings/mapping-limit")

    def write_index_settings(self, output_directory=Path(), use_alias=True, use_ilm=True, use_template=True):
        index = {}
        files = {}

        ilm_policy_name = f"{self.alias}-ilm"
        if use_ilm:
            if not use_alias:
                print("ERROR: Use of ILM requires using aliases for rollover.", file=sys.stderr)
                return
            if not use_template:
                print("WARNING: Recommend use of index templates when using ILM to preserve")
                print("  settings and mappings after rollovers.")
            self.settings["index.lifecycle.name"] = ilm_policy_name
            self.settings["index.lifecycle.rollover_alias"] = self.alias
            files["ilm"] = {"name": f"{ilm_policy_name}.json", "contents": DEFAULT_ILM_POLICY}

        if use_template:
            if not use_alias:
                print("WARNING: Recommend use of aliases when using index templates.")
                print(f"  Assuming that the index match pattern is {self.alias}-*.")
            template = self.get_index_template()
            files["template"] = {"name": f"{self.alias}-template.json", "contents": template}
        else:
            index = self.index_definition

        if use_alias:
            if self.alias[-1].isdigit():
                print(f"WARNING: Alias {self.alias} ends with a number, which is not recommended")
                if "-0" in self.alias:
                    print(f"  Consider using: --se_index_name={'-'.join(self.alias.split('-')[:-1])}")
            index["aliases"] = {self.alias: {"is_write_index": True}}
            index_name = f"{self.alias}-000001"
            print(f"The initial index will be named {index_name}")
        else:
            index_name = self.alias
        files["index"] = {"name": f"{index_name}.json", "contents": index}

        if not output_directory.exists():
            print(f"{output_directory} does not exist, creating it")
            output_directory.mkdir(parents=True)

        curl_put = 'curl -X PUT -u "${ELASTIC_USER}:${ELASTIC_PASSWORD}" -H "Content-Type: application/json"'
        readme = {
            "ilm": f"""ILM policy ({{filename}}) should be PUT to _ilm/policy/{ilm_policy_name}
See: https://www.elastic.co/docs/api/doc/elasticsearch/operation/operation-ilm-put-lifecycle
A default ILM policy was written by condor_adstash, inspect the policy first
and then consider adjusting it now or (e.g. via Kibana) later.
  {curl_put} "${{ELASTIC_BASE_URL}}/_ilm/policy/{ilm_policy_name}" -d @{{filename}}""",
            "template": f"""Index template ({{filename}}) should be PUT to _index_template/{self.index_template_name}
See: https://www.elastic.co/docs/api/doc/elasticsearch/operation/operation-indices-put-index-template
  {curl_put} "${{ELASTIC_BASE_URL}}/_index_template/{self.index_template_name}" -d @{{filename}}""",
            "index": f"""Initial index ({{filename}}) should be PUT to {index_name}
See: https://www.elastic.co/docs/api/doc/elasticsearch/operation/operation-indices-create
  {curl_put} "${{ELASTIC_BASE_URL}}/{index_name}" -d @{{filename}}"""
        }
        readme_path = output_directory / "README"
        if readme_path.exists():
            print(f"ERROR: {readme_path} already exists, please specify an empty directory.", file=sys.stderr)
            return
        with readme_path.open("w") as f:
            if len(files) > 1:
                f.write("""IMPORTANT: These operations should be done in order!
Failure to do so may result in templates and ILM policies not applying.\n\n""")
            for obj_type in ["ilm", "template", "index"]:
                if obj_type in files:
                    f.write(readme[obj_type].replace("{filename}", files[obj_type]["name"]))
                    f.write("\n\n")
            if use_alias:
                f.write(f"NOTE: Since aliases are enabled, the index can be referenced as {self.alias}\n")
                f.write(f"instead of {index_name} when querying or configuring condor_adstash.\n")

        for obj in files.values():
            file_path = output_directory / obj["name"]
            if file_path.exists():
                print(f"ERROR: {file_path} already exists, please specify an empty directory.", file=sys.stderr)
                return
            with file_path.open("w") as f:
                json.dump(obj["contents"], f, indent=2)

        print(f"Index setup files written to {output_directory}")
        print("Remember to remove --init_index before running condor_adstash after setting up your index.")
