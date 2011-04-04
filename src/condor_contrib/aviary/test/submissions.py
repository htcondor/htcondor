#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2009-2011 Red Hat, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# uses Suds - https://fedorahosted.org/suds/
import logging
from suds import *
from suds.client import Client
from sys import exit, argv

# enable these to see the SOAP messages
#logging.basicConfig(level=logging.INFO)
#logging.getLogger('suds.client').setLevel(logging.DEBUG)

query_wsdl = 'file:/usr/services/query/aviary-query.wsdl'

sub_name = len(argv) > 1 and argv[1]
query_url = len(argv) > 2 and argv[2] or 'http://localhost:9091/services/query/getSubmissionSummary'

client = Client(query_wsdl);
client.set_options(location=query_url)

# enable to see service schema
#print client

# set up our ID
if sub_name:
	subId = client.factory.create("ns0:SubmissionID")
	subId.name = sub_name
else:
	# returns all jobs
	subId = None

try:
	submissions = client.service.getSubmissionSummary(subId)
except Exception, e:
	print "invocation failed: ", query_url
	print e
	exit(1)

print submissions
