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
from subprocess import Popen, PIPE
from optparse import OptionParser
from aviary.util import *

# change these for other default locations and ports
wsdl = 'file:/var/lib/condor/aviary/services/query/aviary-query.wsdl'

parser = build_basic_parser('Reconcile job info between HTCondor and Aviary','http://localhost:39091/services/query/getJobStatus')
(opts,args) =  parser.parse_args()

client = create_suds_client(opts,wsdl,None)

client.set_options(location=opts.url)

print "Collecting jobs from Aviary..."
aviary_jobs = []
try:
	jobs = client.service.getJobStatus(None)
	for j in jobs:
		aviary_jobs.append(j.id.job)
except Exception, e:
	print "invocation failed: ", opts.url
	print e
	exit(1)

print "Found %d jobs in Aviary" % len(aviary_jobs)

chproc = Popen('condor_history -format "%s." ClusterId -format "%s," ProcId', shell=True, stdout=PIPE)
history = sorted(chproc.stdout.readline().split(','))
del history[0]

cqproc = Popen('condor_q -format "%s." ClusterId -format "%s," ProcId', shell=True, stdout=PIPE)
live = sorted(cqproc.stdout.readline().split(','))
del live[0]

alljobs = sorted(set(history+live))
print "Checking %d HTCondor jobs " % len(alljobs)
for job in alljobs:
	if not job in aviary_jobs:
		print "No record of ", job

if len(alljobs) == len(aviary_jobs):
	print "All jobs in HTCondor accounted for in Aviary"
else:
	print "Mismatch - inventory check failed"
