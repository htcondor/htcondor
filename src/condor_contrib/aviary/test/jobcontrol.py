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
from aviary.https import *
import argparse

# change these for other default locations and ports
wsdl = 'file:/var/lib/condor/aviary/services/job/aviary-job.wsdl'
key = '/etc/pki/tls/certs/client.key'
cert = '/etc/pki/tls/certs/client.crt'
client = Client(wsdl);

cmds = ['holdJob', 'releaseJob', 'removeJob']

parser = argparse.ArgumentParser(description='Control job state remotely via SOAP.')
parser.add_argument('-q','--quiet', action="store_true",default=False, help='disable/enable SOAP logging')
parser.add_argument('-u','--url', action="store", nargs='?', dest='url',
		    default="http://localhost:9090/services/job/",
		    help='http or https URL prefix to be added to cmd')
parser.add_argument('-k','--key', action="store", nargs='?', dest='key', help='client SSL key file')
parser.add_argument('-c','--cert', action="store", nargs='?', dest='cert', help='client SSL certificate file')
parser.add_argument('cmd', action="store", choices=(cmds))
parser.add_argument('cproc', action="store", help="a cluster.proc id like '1.0' or '5.3'")
args =  parser.parse_args()

if "https://" in args.url:
	client = Client(wsdl,transport = HTTPSClientCertTransport(key,cert))
else:
	client = Client(wsdl)

args.url += args.cmd
client.set_options(location=args.url)

# enable to see service schema
if not args.quiet:
	logging.basicConfig(level=logging.INFO)
	logging.getLogger('suds.client').setLevel(logging.DEBUG)
	print client

# set up our JobID
jobId = client.factory.create('ns0:JobID')
jobId.job = args.cproc

try:
	func = getattr(client.service, args.cmd, None)
	if callable(func):
		print 'invoking', args.url, 'for job', args.cproc
		result = func(jobId,"test")
except Exception, e:
	print "unable to access scheduler at: ", args.url
	print e
	exit(1)

if result.code != "OK":
	print result.code,":", result.text
else:
	print cmd, 'succeeded'
