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
from sys import exit, argv
import time, pwd

uid = pwd.getpwuid(os.getuid())[0]
if not uid:
    uid = "condor"

quiet = False

# change these for other default locations and ports
job_wsdl = 'file:/usr/services/job/aviary-job.wsdl'
job_url = 'http://localhost:9090/services/job/submitJob'

for arg in argv[1:]:
	if arg == '-q':
		quiet = True
	if "http://" in arg:
		url = arg

client = Client(job_wsdl);
client.set_options(location=job_url)

if not quiet:
	print client
	
# add specific requirements here
req1 = client.factory.create("ns0:ResourceConstraint")
req1.type = 'OS'
req1.value = 'LINUX'
reqs = [ req1 ]

# add extra Condor-specific or custom job attributes here
extra1 = client.factory.create("ns0:Attribute")
extra1.name = 'RECIPE'
extra1.type = 'STRING'
extra1.value = 'SECRET_SAUCE'
extras = [ extra1 ]

try:
	result = client.service.submitJob( \
	# the executable command
		'/bin/sleep', \
	# some arguments for the command
		'120', \
	# the submitter name
		uid, \
	# initial working directory wwhere job will execute
		'/tmp', \
	# an arbitrary string identifying the target submission group
		'python_test_submit', \
	# special resource requirements
		reqs,	\
	# additional attributes
		extras
	)
except Exception, e:
	print "invocation failed at: ", job_url
	print e
	exit(1)	

if result.status.code != "OK":
	print result.status.code,"; ", result.status.text
	exit(1)

if not quiet:
	print result
else:
	print result.id.job;
