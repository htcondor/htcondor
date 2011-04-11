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
from sys import exit, argv, stdin
import time

# change these for other default locations and ports
job_wsdl = 'file:/var/lib/condor/aviary/services/job/aviary-job.wsdl'

cproc =  len(argv) > 1 and argv[1]
attr_name = len(argv) > 2 and argv[2]
attr_value = len(argv) > 3 and argv[3]
job_url = len(argv) > 4 and argv[4] or "http://localhost:9090/services/job/setJobAttribute"

client = Client(job_wsdl);
client.set_options(location=job_url)

# set up our JobID
jobId = client.factory.create('ns0:JobID')
jobId.job = cproc

# set up the Attribute
aviary_attr = client.factory.create('ns0:Attribute')
aviary_attr.name = attr_name
aviary_attr.type = "STRING";
aviary_attr.value = '"'+attr_value+'"'

try:
	result = client.service.setJobAttribute(jobId, aviary_attr)
except Exception, e:
	print "unable to access scheduler at: ", job_url
	print e
	exit(1)

if result.code != "OK":
	print result.code,"; ", result.text
