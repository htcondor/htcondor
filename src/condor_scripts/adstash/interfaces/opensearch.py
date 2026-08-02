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

try:
    import opensearchpy
    from opensearchpy import VERSION as OS_VERSION
    _OS_MODULE_FOUND = True
except ModuleNotFoundError as err:
    _OS_MODULE_FOUND = False
    _OS_MODULE_NOT_FOUND_ERROR = err

from adstash.interfaces.elasticsearch import ElasticsearchInterface


if _OS_MODULE_FOUND and (OS_VERSION < (1,0,0) or OS_VERSION >= (3,0,0)):
    logging.warning(f"Unsupported Opensearch Python library {OS_VERSION}, proceeding anyway...")


class OpenSearchInterface(ElasticsearchInterface):
    """
    The OpenSearch interface is the same as the Elasticsearch interface
    with a few methods overridden to use the opensearchpy library API.
    """


    def __init__(
            self,
            _check_for_module=True,
            **kwargs
            ):
        if _check_for_module and not _OS_MODULE_FOUND:  # raise module not found error if missing
            raise _OS_MODULE_NOT_FOUND_ERROR
        super().__init__(_check_for_module=False, **kwargs)


    def get_handle(self) -> opensearchpy.OpenSearch:
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


    def update_mappings(self, index: str, mappings: dict, **kwargs):
        """
        Given an index and mappings, push the new mapping to the index
        """
        client = self.get_handle()

        logging.info(f"Updating mappings for index {index}")
        logging.debug(json.dumps(mappings, indent=2))
        if OS_VERSION >= (2,0,0):
            body = {"mappings": mappings}
            client.indices.put_mapping(index=index, body=body)
        else:
            client.indices.put_mapping(index=index, **mappings)
