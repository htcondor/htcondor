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
import argparse
from aviary.https import *

# change these for other default locations and ports
wsdl = 'file:/var/lib/condor/aviary/services/query/aviary-query.wsdl'
url = 'http://localhost:9091/services/query/'
key = '/etc/pki/tls/certs/client.key'
cert = '/etc/pki/tls/certs/client.crt'
cmds = ['getJobStatus', 'getJobSummary', 'getJobDetails']
client = Client(wsdl);

parser = argparse.ArgumentParser(description='Query jobs remotely via SOAP.')
parser.add_argument('-v','--verbose', action="store_true",default=False, help='enable SOAP logging')
parser.add_argument('-u','--url', action="store", nargs='?', dest='url', help='http or https URL prefix to be added to cmd')
parser.add_argument('-k','--key', action="store", nargs='?', dest='key', help='client SSL key file')
parser.add_argument('-c','--cert', action="store", nargs='?', dest='cert', help='client SSL certificate file')
parser.add_argument('cmd', action="store", choices=(cmds))
parser.add_argument('cproc', action="store", help="a cluster.proc id like '1.0' or '5.3'")
args =  parser.parse_args()

if args.url and "https://" in args.url:
	url = args.url
	client = Client(wsdl,transport = HTTPSClientCertTransport(key,cert))

url += args.cmd
client.set_options(location=url)

# enable to see service schema
if args.verbose:
	logging.basicConfig(level=logging.INFO)
	logging.getLogger('suds.client').setLevel(logging.DEBUG)
	print client

# set up our JobID
if args.cproc:
	jobId = client.factory.create("ns0:JobID")
	jobId.job = args.cproc
else:
	# returns all jobs
	jobId = None

try:
	func = getattr(client.service, args.cmd, None)
	if callable(func):
		print 'invoking', url, 'for job', args.cproc
		result = func(jobId)
except Exception, e:
	print "invocation failed: ", url
	print e
	exit(1)

print result
