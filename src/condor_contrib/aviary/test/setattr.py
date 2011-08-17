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
wsdl = 'file:/var/lib/condor/aviary/services/job/aviary-job.wsdl'
url = "http://localhost:9090/services/job/setJobAttribute"
key = '/etc/pki/tls/certs/client.key'
cert = '/etc/pki/tls/certs/client.crt'
types = ['BOOLEAN','EXPRESSION','FLOAT','INTEGER','STRING']
client = Client(wsdl);

parser = argparse.ArgumentParser(description='Set job attributes remotely via SOAP.')
parser.add_argument('-v','--verbose', action="store_true",default=False, help='enable SOAP logging')
parser.add_argument('-u','--url', action="store", nargs='?', dest='url', help='http or https URL prefix to be added to cmd')
parser.add_argument('-k','--key', action="store", nargs='?', dest='key', help='client SSL key file')
parser.add_argument('-c','--cert', action="store", nargs='?', dest='cert', help='client SSL certificate file')
parser.add_argument('name', action="store", help='attribute name')
parser.add_argument('type', action="store", choices=(types), help='attribute type')
parser.add_argument('value', action="store", help='attribute value')
parser.add_argument('cproc', action="store", help="a cluster.proc id like '1.0' or '5.3'")
args =  parser.parse_args()

if args.url and "https://" in args.url:
	url = args.url
	client = Client(wsdl,transport = HTTPSClientCertTransport(key,cert))

client.set_options(location=url)

# enable to see service schema
if args.verbose:
	logging.basicConfig(level=logging.INFO)
	logging.getLogger('suds.client').setLevel(logging.DEBUG)
	print client

# set up our JobID
jobId = client.factory.create('ns0:JobID')
jobId.job = args.cproc

# set up the Attribute
aviary_attr = client.factory.create('ns0:Attribute')
aviary_attr.name = args.name
aviary_attr.type = args.type
aviary_attr.value = args.value

try:
	print 'invoking', url, 'for job', args.cproc
	result = client.service.setJobAttribute(jobId, aviary_attr)
except Exception, e:
	print "unable to access scheduler at: ", url
	print e
	exit(1)

if result.code != "OK":
	print result.code,":", result.text
