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
import random
import pprint
import logging

from pathlib import Path
from operator import itemgetter
from collections import defaultdict

try:
    import elasticsearch
    from elasticsearch import VERSION as ES_VERSION
    _ES_MODULE_FOUND = True
except ModuleNotFoundError as err:
    _ES_MODULE_FOUND = False
    _ES_MODULE_NOT_FOUND_ERROR = err

import adstash.convert as convert
from adstash.utils import get_host_port
from adstash.interfaces.generic import GenericInterface

ES8 = (8,0,0)
if _ES_MODULE_FOUND and (ES_VERSION < (7,0,0) or ES_VERSION >= (9,0,0)):
    logging.warning(f"Unsupported Elasticsearch Python library {ES_VERSION}, proceeding anyway...")


class ElasticsearchInterface(GenericInterface):


    def __init__(
            self,
            host="localhost",
            port="9200",
            url_prefix="",
            username=None,
            password=None,
            use_https=False,
            ca_certs=None,
            timeout=60,
            **kwargs
            ):
        if not _ES_MODULE_FOUND:  # raise module not found error if missing
            raise _ES_MODULE_NOT_FOUND_ERROR
        self.host, self.port = get_host_port(host, port)
        self.url_prefix = url_prefix or ""
        self.username = username
        self.password = password
        self.use_https = use_https
        self.ca_certs = ca_certs
        self.timeout = timeout
        self.handle = None


    def get_handle(self):
        if self.handle is not None:
            return self.handle

        client_options = {}

        if ES_VERSION < ES8:
            client_options["hosts"] = [{
                "host": self.host,
                "port": self.port,
                "url_prefix": self.url_prefix,
                "use_ssl": self.use_https,
                }]
        elif ES_VERSION >= ES8:
            client_options["hosts"] = f"""http{"s" * self.use_https}://{self.host}:{self.port}/{self.url_prefix}"""

        if (self.username is None) and (self.password is None):
            pass  # anonymous auth
        elif (self.username is None) != (self.password is None):
            logging.warning("Only one of username and password have been defined, attempting anonymous connection to Elasticsearch")
        else:  # basic auth
            auth_tuple = (self.username, self.password,)
            if ES_VERSION < ES8:
                client_options["http_auth"] = auth_tuple
            elif ES_VERSION >= ES8:
                client_options["basic_auth"] = auth_tuple

        if self.ca_certs is not None:
            client_options["ca_certs"] = self.ca_certs
        if self.use_https:
            client_options["verify_certs"] = True

        client_options["timeout"] = self.timeout

        self.handle = elasticsearch.Elasticsearch(**client_options)
        return self.handle


    def get_active_index(self, alias):
        client = self.get_handle()
        indices = client.indices.get_alias(name=alias)

        # find which index is reporting as writable
        for index, alias_info in indices.items():
            if alias_info["aliases"][alias]["is_write_index"]:
                logging.info(f"{alias} is an alias, found active index {index}.")
                return index

        # fallback to lexigraphically last index
        indices = list(indices.keys())
        indices.sort(reverse=True)
        logging.warning(f"Could not find an activate index for alias {alias}, trying {indices[0]}")
        return indices[0]


    def get_mappings(self, index):
        client = self.get_handle()
        return client.indices.get_mapping(index=index)[index]["mappings"]


    def make_mappings(self):
        properties = {}

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

        properties["metadata"] = {
            "properties": {
                "adstash_runtime": {"type": "date", "format": "epoch_second"},
                "condor_history_runtime": {"type": "date", "format": "epoch_second"},
            }
        }

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

        mappings = {
            "dynamic_templates": dynamic_templates,
            "properties": properties,
            "date_detection": False,
            "numeric_detection": False,
        }

        return mappings


    def make_settings(self):
        settings = {
            "analysis": {
                "analyzer": {
                    "analyzer_keyword": {"tokenizer": "keyword", "filter": "lowercase"}
                }
            },
            "mapping.total_fields.limit": 5000,
        }
        return settings


    def setup_index(self, index, log_mappings=True, log_dir=Path.cwd(), **kwargs):
        client = self.get_handle()

        # check if index is an alias, and get the active index if so
        if client.indices.exists_alias(name=index):
            index = self.get_active_index(index)

        mappings = self.make_mappings()
        settings = self.make_settings()

        if not client.indices.exists(index=index):  # push new index if doesn't exist
            logging.info(f"Creating new index {index}.")
            client.indices.create(index=index, mappings=mappings, settings=settings)
            if log_mappings and log_dir:
                mappings_file = log_dir / "condor_adstash_elasticsearch_last_mappings.json"
                logging.debug(f"Writing new mappings to {mappings_file}.")
                json.dump(mappings, open(mappings_file, "w"), indent=2)
            return

        # otherwise check existing index for missing mappings properties
        update_mappings = False
        updated_mappings = {}
        existing_mappings = self.get_mappings(index)
        for outer_key in mappings:
            if outer_key not in existing_mappings:  # add anything missing
                updated_mappings[outer_key] = mappings[outer_key]
                update_mappings = True
            elif isinstance(mappings[outer_key], dict):  # update missing keys in any existing dicts
                missing_inner_keys = set(mappings[outer_key]) - set(existing_mappings[outer_key])
                if len(missing_inner_keys) > 0:
                    updated_mappings[outer_key] = {}
                    for inner_key in missing_inner_keys:
                        updated_mappings[outer_key][inner_key] = mappings[outer_key][inner_key]
                    update_mappings = True
        if update_mappings:
            logging.info(f"Updated mappings for index {index}")
            logging.debug(f"{pprint.pformat(updated_mappings)}")
            if ES_VERSION < ES8:
                client.indices.put_mapping(index=index, body=json.dumps(updated_mappings))
            elif ES_VERSION >= ES8:
                client.indices.put_mapping(index=index, **updated_mappings)
            if log_mappings and log_dir:
                mappings_file = log_dir / "condor_adstash_elasticsearch_last_mappings.json"
                logging.debug(f"Writing updated mappings to {mappings_file}.")
                json.dump(updated_mappings, open(mappings_file, "w"), indent=2)


    def make_bulk_body(self, ads, metadata={}):
        body = ""
        for doc_id, ad in ads:
            if metadata:
                ad.setdefault("metadata", {}).update(metadata)
            action = {"index": {"_id": doc_id}}
            body += f"{json.dumps(action)}\n{json.dumps(ad)}\n"
        return body


    def get_error_info(self, result):
        if not result["errors"]:
            return 0

        n_errors = 0
        error_types = defaultdict(int)
        error_reasons = []
        for item in result["items"]:
            try:
                error = item["index"]["error"]
                n_errors += 1
            except (KeyError, TypeError):
                continue
            try:
                error_reasons.append(error["reason"])
            except (KeyError, TypeError):
                pass
            try:
                error_type = error["type"]
            except (KeyError, TypeError):
                error_type = "unknown"
            error_types[error_type] += 1

        error_type_list = list(error_types.items())
        error_type_list.sort(key=itemgetter(1), reverse=True)
        error_type_strs = []
        for (error_type, n) in error_type_list[:3]:
            error_type_strs.append(f"{error_type} ({n} times)")
        logging.error(f"{n_errors} errors encountered during bulk index.")
        logging.error(f"""Most common error type(s): {", ".join(error_type_strs)}.""")
        try:
            logging.error(f"""Example reason: {random.choice(error_reasons)}.""")
        except IndexError:
            pass

        return n_errors


    def post_ads(self, ads, index, metadata={}, **kwargs):
        client = self.get_handle()

        self.setup_index(index, **kwargs)
        body = self.make_bulk_body(ads, metadata)
        result = client.bulk(body=body, index=index)
        n_errors = self.get_error_info(result)
        return {"success": len(ads)-n_errors, "error": n_errors}
