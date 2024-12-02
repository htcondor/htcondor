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

import logging
import json
import pprint

from pathlib import Path

try:
    import opensearchpy
    from opensearchpy import VERSION as OS_VERSION
    _OS_MODULE_FOUND = True
except ModuleNotFoundError as err:
    _OS_MODULE_FOUND = False
    _OS_MODULE_NOT_FOUND_ERROR = err


from adstash.utils import get_host_port
from adstash.interfaces.elasticsearch import ElasticsearchInterface


if _OS_MODULE_FOUND and (OS_VERSION < (1,0,0) or OS_VERSION >= (3,0,0)):
    logging.warning(f"Unsupported Opensearch Python library {OS_VERSION}, proceeding anyway...")


class OpenSearchInterface(ElasticsearchInterface):


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
        if not _OS_MODULE_FOUND:  # raise module not found error if missing
            raise _OS_MODULE_NOT_FOUND_ERROR
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
        client_options["hosts"] = [{
            "host": self.host,
            "port": self.port,
            "url_prefix": self.url_prefix,
            "use_ssl": self.use_https,
        }]

        if (self.username is None) and (self.password is None):
            pass  # anonymous auth
        elif (self.username is None) != (self.password is None):
            logging.warning("Only one of username and password have been defined, attempting anonymous connection to OpenSearch")
        else:  # basic auth
            auth_tuple = (self.username, self.password,)
            client_options["http_auth"] = auth_tuple

        if self.ca_certs is not None:
            client_options["ca_certs"] = self.ca_certs
        if self.use_https:
            client_options["verify_certs"] = True

        client_options["timeout"] = self.timeout

        self.handle = opensearchpy.OpenSearch(**client_options)
        return self.handle


    def setup_index(self, index, log_mappings=True, log_dir=Path.cwd(), **kwargs):
        client = self.get_handle()

        # check if index is an alias, and get the active index if so
        if client.indices.exists_alias(name=index):
            index = self.get_active_index(index)

        mappings = self.make_mappings(**kwargs)
        settings = self.make_settings(**kwargs)

        if not client.indices.exists(index=index):  # push new index if doesn't exist
            logging.info(f"Creating new index {index}.")
            if OS_VERSION >= (2,0,0):
                body = {
                    "mappings": mappings,
                    "settings": settings,
                }
                client.indices.create(index=index, body=json.dumps(body))
            else:
                client.indices.create(index=index, mappings=mappings, settings=settings)
            if log_mappings and log_dir:
                mappings_file = log_dir / "condor_adstash_opensearch_last_mappings.json"
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
            if OS_VERSION >= (2,0,0):
                body = {"mappings": updated_mappings}
                client.indices.put_mapping(index=index, body=body)
            else:
                client.indices.put_mapping(index=index, **updated_mappings)
            if log_mappings and log_dir:
                mappings_file = log_dir / "condor_adstash_opensearch_last_mappings.json"
                logging.debug(f"Writing updated mappings to {mappings_file}.")
                json.dump(updated_mappings, open(mappings_file, "w"), indent=2)
