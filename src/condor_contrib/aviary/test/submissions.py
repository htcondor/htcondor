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
from suds import *
from suds.client import Client
from sys import exit
from optparse import OptionParser
from aviary.util import *

wsdl = 'file:/var/lib/condor/aviary/services/query/aviary-query.wsdl'

parser = build_basic_parser('Query submission summaries remotely via SOAP.','http://localhost:39091/services/query/getSubmissionSummary')
parser.add_option('--name', action="store", dest='name', help='submission name')
(opts,args) =  parser.parse_args()

client = create_suds_client(opts,wsdl,None)
client.set_options(location=opts.url)

# set up our ID
if opts.name:
	subId = client.factory.create("ns0:SubmissionID")
	subId.name = opts.name
else:
	# returns all submissions
	subId = None

try:
	submissions = client.service.getSubmissionSummary(subId)
except Exception, e:
	print "invocation failed: ", opts.url
	print e
	exit(1)

print submissions
