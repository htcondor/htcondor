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

# change these for other default locations and ports
wsdl = 'file:/var/lib/condor/aviary/services/job/aviary-job.wsdl'
types = ['BOOLEAN','EXPRESSION','FLOAT','INTEGER','STRING']

parser = build_basic_parser('Set job attributes remotely via SOAP.','http://localhost:39090/services/job/setJobAttribute')
parser.add_option('--name', action="store", dest='name', help='attribute name')
parser.add_option('--type', action="store", choices=(types), dest='type', help=str(types))
parser.add_option('--value', action="store", dest='value', help='attribute value')
parser.add_option('--cproc', action="store", dest='cproc', help="a cluster.proc id like '1.0' or '5.3'")
(opts,args) =  parser.parse_args()

# the joy that is optparse...
if opts.name is None:
	print 'You must provide an attribute name'
	parser.print_help()
	exit(1)

if opts.type is None:
	print 'You must provide an attribute type'
	parser.print_help()
	exit(1)

if opts.value is None:
	print 'You must provide an attribute value'
	parser.print_help()
	exit(1)

if opts.cproc is None:
	print 'You must provide a cluster.proc job id'
	parser.print_help()
	exit(1)

client = create_suds_client(opts,wsdl,None)
client.set_options(location=opts.url)

# set up our JobID
jobId = client.factory.create('ns0:JobID')
jobId.job = opts.cproc

# set up the Attribute
aviary_attr = client.factory.create('ns0:Attribute')
aviary_attr.name = opts.name
aviary_attr.type = opts.type
aviary_attr.value = opts.value

try:
	print 'invoking', opts.url, 'for job', opts.cproc
	result = client.service.setJobAttribute(jobId, aviary_attr)
except Exception, e:
	print "unable to access scheduler at: ", opts.url
	print e
	exit(1)

if result.code != "OK":
	print result.code,":", result.text
