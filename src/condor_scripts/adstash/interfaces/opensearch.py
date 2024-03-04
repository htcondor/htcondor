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

import opensearchpy

from adstash.utils import get_host_port
from adstash.interfaces.elasticsearch import ElasticsearchInterface


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
