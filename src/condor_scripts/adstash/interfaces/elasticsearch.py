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
import logging

from operator import itemgetter
from collections import defaultdict

try:
    import elasticsearch
    from elasticsearch import VERSION as ES_VERSION
    _ES_MODULE_FOUND = True
except ModuleNotFoundError as err:
    _ES_MODULE_FOUND = False
    _ES_MODULE_NOT_FOUND_ERROR = err

from adstash.utils import get_host_port, classad_json_serializer
from adstash.interfaces.generic import GenericInterface

ES8 = (8,0,0)
if _ES_MODULE_FOUND and (ES_VERSION < (7,0,0) or ES_VERSION >= (9,0,0)):
    logging.warning(f"Unsupported Elasticsearch Python library {ES_VERSION}, proceeding anyway...")


class ElasticsearchInterface(GenericInterface):

    is_search_engine = True

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
            _check_for_module=True,
            **kwargs
            ):
        if _check_for_module and not _ES_MODULE_FOUND:  # raise module not found error if missing
            raise _ES_MODULE_NOT_FOUND_ERROR
        self.host, self.port = get_host_port(host, port)
        self.url_prefix = url_prefix or ""
        self.username = username
        self.password = password
        self.use_https = use_https
        self.ca_certs = ca_certs
        self.timeout = timeout
        self.handle = None
        super().__init__(**kwargs)


    def __getstate__(self):
        """Remove handle to make object pickleable"""
        state = self.__dict__.copy()
        state["handle"] = None
        return state


    def get_handle(self) -> elasticsearch.Elasticsearch:
        """
        Set up the Elasticsearch client if needed.
        """
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


    def ping(self) -> None:
        client = self.get_handle()
        if not client.ping():
            raise ConnectionError(f"Could not connect to {self.__class__.__name__} at {self.host}:{self.port}")


    def get_health(self) -> dict:
        client = self.get_handle()
        health = {}
        try:
            health = client.cluster.health()
        except elasticsearch.exceptions.AuthorizationException:
            logging.warning(f"Search engine user {self.username} does not have cluster-level access, cannot get health status")
        except Exception as e:
            logging.exception(f"Cannot get health status due to error")
        return health


    def get_active_index(self, alias: str) -> str:
        client = self.get_handle()
        try:
            indices = client.indices.get_alias(name=alias)
        except elasticsearch.exceptions.NotFoundError:
            logging.info(f"{alias} is not an alias, assuming {alias} is the active index")
            return alias

        # find which index is reporting as writable
        for index, alias_info in indices.items():
            if alias_info["aliases"][alias].get("is_write_index"):
                logging.info(f"{alias} is an alias, found active index {index}.")
                return index

        # fallback to lexigraphically last index
        indices = list(indices.keys())
        indices.sort(reverse=True)
        logging.warning(f"Could not find an activate index for alias {alias}, trying {indices[0]}")
        return indices[0]


    def get_mappings(self, index: str) -> dict:
        """
        Fetch the existing mappings for an index (if it exists)
        """
        client = self.get_handle()
        mappings = {}
        try:
            mappings = client.indices.get_mapping(index=index)[index]["mappings"]
        except elasticsearch.exceptions.NotFoundError:
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
        if ES_VERSION < ES8:
            client.indices.put_mapping(index=index, body=json.dumps(mappings))
        elif ES_VERSION >= ES8:
            client.indices.put_mapping(index=index, **mappings)


    def update_settings(self, index: str, settings: dict, **kwargs):
        """
        Given an index and settings, push the new settings to the index
        """
        client = self.get_handle()

        logging.info(f"Updating settings for index {index}")
        logging.debug(json.dumps(settings, indent=2))
        if ES_VERSION < ES8:
            client.indices.put_settings(index=index, body=json.dumps(settings))
        elif ES_VERSION >= ES8:
            client.indices.put_settings(index=index, settings=settings)


    def make_bulk_body(self, docs: list, metadata={}) -> str:
        """
        Elasticsearch supports bulk indexing via NDJSON, where
        an action (e.g. "index") is followed by the data object
        being acted upon.
        https://www.elastic.co/docs/api/doc/elasticsearch/operation/operation-bulk
        """
        body = []
        for doc_id, doc in docs:
            doc["metadata"] = {**doc.get("metadata", {}), **metadata}  # merge existing with chunk-level metadata
            action = {"index": {"_id": doc_id}}  # index the doc w/ this id
            body.append(json.dumps(action))
            body.append(json.dumps(doc, sort_keys=True, default=classad_json_serializer))
        return "\n".join(body)


    def get_error_count(self, result: dict, n_ads: int = 0, raise_on_errors: bool = False) -> int:
        """
        Crawl through the result from the bulk API,
        print out any errors,
        and return the number of errors encountered.
        https://www.elastic.co/docs/api/doc/elasticsearch/operation/operation-bulk#operation-bulk-200

        n_ads is the number of docs submitted; used as the error count when the
        response is malformed (missing 'errors' key), since we cannot confirm any
        docs were indexed successfully.

        If raise_on_errors is True, raise RuntimeError instead of returning when
        the response is malformed or when indexing errors are present.
        """
        if "errors" not in result:
            msg = f"Bulk response missing 'errors' key (possible timeout or partial response): {result}"
            if raise_on_errors:
                raise RuntimeError(msg)
            logging.warning(msg)
            return n_ads

        if not result["errors"]:
            return 0

        took = result.get("took")
        items = result.get("items", [])
        n_success = sum(1 for item in items if item.get("index", {}).get("status", 0) < 300)

        if n_success == 0 and not items:
            logging.error(f"Bulk response has errors=true but no items; raw result: {result}")

        n_errors = 0
        error_types = defaultdict(int)
        error_reasons = []
        for item in items:
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
        took_str = f", took {took}ms on ES side" if took is not None else ""
        logging.error(f"{n_errors} errors encountered during bulk index ({n_success} succeeded{took_str}).")
        logging.error(f"""Most common error type(s): {", ".join(error_type_strs)}.""")
        try:
            logging.error(f"""Example reason: {random.choice(error_reasons)}.""")
        except IndexError:
            pass

        if raise_on_errors:
            raise RuntimeError(
                f"{n_errors} errors in bulk index ({n_success} succeeded{took_str}); "
                f"most common type(s): {', '.join(error_type_strs)}"
            )

        return n_errors


    def post_ads(self, ads: list, index: str, metadata={}, **kwargs) -> dict:
        """
        Push a list of JSON-ified ads in the format
        [(doc_id, ad), (doc_id, ad), ...]
        to the given Elasticsearch index.
        """
        client = self.get_handle()

        body = self.make_bulk_body(ads, metadata)
        result = client.bulk(body=body, index=index, filter_path=["errors", "took", "items.*.index.error.**", "items.*.index.status"])
        n_errors = self.get_error_count(result, n_ads=len(ads), **kwargs)
        return {"success": len(ads)-n_errors, "error": n_errors}


