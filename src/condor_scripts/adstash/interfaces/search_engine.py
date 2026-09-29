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
import random
import logging

from operator import itemgetter
from collections import defaultdict

from adstash.utils import get_host_port, classad_json_serializer
from adstash.interfaces.generic import GenericInterface


class SearchEngineInterface(GenericInterface):
    """
    Base class for search engine interfaces (Elasticsearch, OpenSearch).
    Contains shared connection setup and library-agnostic helper methods.
    """

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
            **kwargs
            ):
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


    def ping(self) -> None:
        client = self.get_handle()
        if not client.ping():
            raise ConnectionError(f"Could not connect to {self.__class__.__name__} at {self.host}:{self.port}")


    def resolve_alias(self, indices: dict, alias: str) -> str:
        """
        Given the result of a get_alias call, find the active index.
        Prefers the index marked as write_index, falls back to
        lexicographically last.
        """
        # find which index is reporting as writable
        for index, alias_info in indices.items():
            if alias_info["aliases"][alias].get("is_write_index"):
                logging.info(f"{alias} is an alias, found active index {index}.")
                return index

        # fallback to lexicographically last index
        indices = list(indices.keys())
        indices.sort(reverse=True)
        logging.warning(f"Could not find an active index for alias {alias}, trying {indices[0]}")
        return indices[0]


    def make_bulk_body(self, docs: list, metadata=None) -> str:
        """
        Search engines support bulk indexing via NDJSON, where
        an action (e.g. "index") is followed by the data object
        being acted upon.
        """
        body = []
        for doc_id, doc in docs:
            doc["metadata"] = {**doc.get("metadata", {}), **(metadata or {})}  # merge existing with chunk-level metadata
            action = {"index": {"_id": doc_id}}  # index the doc w/ this id
            body.append(json.dumps(action))
            body.append(json.dumps(doc, sort_keys=True, default=classad_json_serializer))
        return "\n".join(body)


    def get_error_count(self, result: dict, n_ads: int = 0, raise_on_errors: bool = False) -> int:
        """
        Crawl through the result from the bulk API,
        print out any errors,
        and return the number of errors encountered.

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
