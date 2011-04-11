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
from sys import exit, argv, stdin
import time

# enable these to see the SOAP messages
#logging.basicConfig(level=logging.INFO)
#logging.getLogger('suds.client').setLevel(logging.DEBUG)

# change these for other default locations and ports
job_wsdl = 'file:/var/lib/condor/aviary/services/query/aviary-query.wsdl'

cmds = ['getJobStatus', 'getJobSummary', 'getJobDetails']

cmdarg = len(argv) > 1 and argv[1]
cproc =  len(argv) > 2 and argv[2]
job_url = len(argv) > 3 and argv[3] or "http://localhost:9091/services/query/"

if cmdarg not in cmds:
	print "error unknown command: ", cmdarg
	print "available commands are: ",cmds
	exit(1)

client = Client(job_wsdl);
job_url += cmdarg
client.set_options(location=job_url)

# enable to see service schema
#print client

# set up our JobID
if cproc:
	jobId = client.factory.create("ns0:JobID")
	jobId.job = cproc
else:
	# returns all jobs
	jobId = None

try:
	func = getattr(client.service, cmdarg, None)
	if callable(func):
	    result = func(jobId)
except Exception, e:
	print "invocation failed: ", job_url
	print e
	exit(1)

print result
