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
import logging

import opensearchpy
from opensearchpy import VERSION as OS_VERSION

from adstash.interfaces.search_engine import SearchEngineInterface

if OS_VERSION < (1,0,0) or OS_VERSION >= (3,0,0):
    logging.warning(f"Unsupported Opensearch Python library {OS_VERSION}, proceeding anyway...")


class OpenSearchInterface(SearchEngineInterface):

    def __init__(
            self,
            **kwargs
            ):
        super().__init__(**kwargs)


    def get_handle(self) -> "opensearchpy.OpenSearch":
        """
        Set up the OpenSearch client if needed.
        """
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


    def get_health(self) -> dict:
        client = self.get_handle()
        health = {}
        try:
            health = client.cluster.health()
        except opensearchpy.exceptions.AuthorizationException:
            logging.warning(f"Search engine user {self.username} does not have cluster-level access, cannot get health status")
        except Exception as e:
            logging.exception(f"Cannot get health status due to error: {e}")
        return health


    def get_active_index(self, alias: str) -> str:
        client = self.get_handle()
        try:
            indices = client.indices.get_alias(name=alias)
        except opensearchpy.exceptions.NotFoundError:
            logging.info(f"{alias} is not an alias, assuming {alias} is the active index")
            return alias
        return self.resolve_alias(indices, alias)


    def get_mappings(self, index: str) -> dict:
        """
        Fetch the existing mappings for an index (if it exists)
        """
        client = self.get_handle()
        mappings = {}
        try:
            mappings = client.indices.get_mapping(index=index)[index]["mappings"]
        except opensearchpy.exceptions.NotFoundError:
            logging.warning(f"Index {index} was not found, assuming no existing mappings")
        return mappings


    def get_settings(self, index: str) -> dict:
        """
        Fetch the existing settings for an index
        """
        client = self.get_handle()
        return client.indices.get_settings(index=index)[index]["settings"]


    def update_mappings(self, index: str, mappings: dict, **kwargs):
        """
        Given an index and mappings, push the new mapping to the index
        """
        client = self.get_handle()

        logging.info(f"Updating mappings for index {index}")
        logging.debug(json.dumps(mappings, indent=2))
        if OS_VERSION >= (2,0,0):
            client.indices.put_mapping(index=index, body=mappings)
        else:
            client.indices.put_mapping(index=index, **mappings)


    def update_settings(self, index: str, settings: dict, **kwargs):
        """
        Given an index and settings, push the new settings to the index
        """
        client = self.get_handle()

        logging.info(f"Updating settings for index {index}")
        logging.debug(json.dumps(settings, indent=2))
        client.indices.put_settings(index=index, body=json.dumps(settings))


    def post_ads(self, ads: list, index: str, metadata=None, **kwargs) -> dict:
        """
        Push a list of JSON-ified ads in the format
        [(doc_id, ad), (doc_id, ad), ...]
        to the given OpenSearch index.
        """
        client = self.get_handle()

        body = self.make_bulk_body(ads, metadata)
        result = client.bulk(body=body, index=index, filter_path=["errors", "took", "items.*.index.error.**", "items.*.index.status"])
        n_errors = self.get_error_count(result, n_ads=len(ads), **kwargs)
        return {"success": len(ads)-n_errors, "error": n_errors}
