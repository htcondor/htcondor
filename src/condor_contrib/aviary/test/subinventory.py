#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2009-2013 Red Hat, Inc.
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
from pprint import pprint
from datetime import *

# change these for other default locations and ports
wsdl = 'file:/var/lib/condor/aviary/services/query/aviary-query.wsdl'

parser = build_basic_parser('Reconcile job info between HTCondor and Aviary','http://localhost:39091/services/query/getSubmissionSummary')
(opts,args) =  parser.parse_args()

client = create_suds_client(opts,wsdl,None)

client.set_options(location=opts.url)

print "Collecting submissions from Aviary..."
aviary_subs = {}
try:
	subs = client.service.getSubmissionSummary(None)
	for s in subs:
		aviary_subs[str(s.id.name)] = s.id.qdate
except Exception, e:
	print "invocation failed: ", opts.url
	print e
	exit(1)

print "Found %d unique submissions in Aviary" % len(aviary_subs)

chproc = Popen('condor_history -format "%s=" Submission -format "%d;" QDate', shell=True, stdout=PIPE)
temp = sorted(chproc.stdout.readline().split(';'))
del temp[0]
history = list(item.split("=") for item in temp)

cqproc = Popen('condor_q -format "%s=" Submission -format "%d;" QDate', shell=True, stdout=PIPE)
temp = sorted(cqproc.stdout.readline().split(';'))
del temp[0]
live = list(item.split("=") for item in temp)

ht_subs = sorted(history+live)

allsubs = {}
for (k,v) in iter(ht_subs):
    allsubs[k] = (allsubs.get(k,0)) or int(v)

print "Checking %d unique HTCondor submissions " % len(allsubs)

for sub in allsubs:
    if not sub in aviary_subs:
        print "No record of ", sub
    if aviary_subs[sub] != allsubs[sub]:
        print "Qdate mismatch at %s with %s vs %d" % (sub, aviary_subs[sub], allsubs[sub])

if len(allsubs) == len(aviary_subs):
    print "All submissions in HTCondor accounted for in Aviary"
else:
	print "Mismatch - inventory check failed"
