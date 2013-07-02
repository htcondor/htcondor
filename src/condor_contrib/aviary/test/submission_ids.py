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
modes = ['AFTER', 'BEFORE']

parser = build_basic_parser('Query submission IDs remotely via SOAP.','http://localhost:39091/services/query/getSubmissionID')
parser.add_option('--size', action="store", dest='size', help='page size')
parser.add_option('--name', action="store", dest='name', help='a name index')
parser.add_option('--qdate', action="store", dest='qdate', help='a qdate index')
parser.add_option('--mode', action="store", choices=(modes), dest='mode', help=str(modes)+ \
    ' relative from the given qdate')
(opts,args) =  parser.parse_args()

if opts.size is None:
    print 'A page size must be supplied'
    parser.print_help()
    exit(1)

client = create_suds_client(opts,wsdl,None)
client.set_options(location=opts.url)

scanMode = None
subId = None

# set up our ID and scanMode
if opts.mode and opts.qdate:
    scanMode = client.factory.create("ns0:ScanMode")
    scanMode = opts.mode
    subId = client.factory.create("ns0:SubmissionID")
    subId.qdate = opts.qdate
elif opts.name and opts.qdate:
    print 'Either name (lexical) OR qdate (time) can be specified for index, not both'
    parser.print_help()
    exit(1)
elif opts.mode and opts.name:
    print "Mode arg incompatible with name arg"
    parser.print_help()
    exit(1)
elif opts.qdate:
    print "Mode arg must be specified with qdate arg"
    parser.print_help()
    exit(1)
elif opts.mode:
    print "Qdate arg must be specified with mode arg"
    parser.print_help()
    exit(1)
# we only scan forward if using a name
elif opts.name:
    subId = client.factory.create("ns0:SubmissionID")
    subId.name = opts.name

# sanity check for qdate
if (opts.qdate and long(opts.qdate) > sys.maxint):
    print 'Invalid input: Qdate is larger than system max integer'
    exit(1)

try:
    submissions = client.service.getSubmissionID(opts.size,scanMode,subId)
except Exception, e:
    print "invocation failed: ", opts.url
    print e
    exit(1)

print submissions
