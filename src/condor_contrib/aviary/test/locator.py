#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2009-2012 Red Hat, Inc.
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
import time, pwd, os, logging
from aviary.util import *

plugins = []
# NOTE: Suds has had little support for adding attributes
# to the request body until 0.4.1
# uncomment the following to disable the partialMatches attribute
# i.e., strict 'name' matching

#from suds.plugin import MessagePlugin
#from suds.sax.attribute import Attribute
#class OverridesPlugin(MessagePlugin):
    #def marshalled(self, context):
        #sj_body = context.envelope.getChild('Body')[0]
        #sj_body.attributes.append(Attribute("partialMatches", "false"))
#plugins=[OverridesPlugin()]

# change for other some WSDL location
wsdl = 'file:/var/lib/condor/aviary/services/locator/aviary-locator.wsdl'
res_types = ['ANY','COLLECTOR','CUSTOM','MASTER','NEGOTIATOR','SCHEDULER','SLOT']

parser = build_basic_parser('Retrieve Aviary SOAP endpoints.','http://localhost:39000/services/locator/locate')
parser.add_option('--type', action="store", choices=(res_types), dest='type', help=str(res_types))
parser.add_option('--custom', action="store", help="a custom sub-type name like 'QUERY_SERVER' (no value will return all for CUSTOM)")
parser.add_option('--name', action="store", help="a hostname to match (will match any part by default)")
(opts,args) =  parser.parse_args()

# eventually could be dealing with thousands of endpoints
# need to explicitly declare classification, even if its ANY
if opts.type is None:
	print 'One of these resource types must be supplied', res_types
	parser.print_help()
	exit(1)

client = create_suds_client(opts,wsdl,plugins)
client.set_options(location=opts.url)

res_id = client.factory.create("ns0:ResourceID")
res_id.resource = opts.type
res_id.sub_type = opts.custom
res_id.name = opts.name

try:
	result = client.service.locate(res_id)
except Exception, e:
	print "invocation failed at: ", opts.url
	print e
	exit(1)	

if result.status.code != "OK":
	print result.status.code,"; ", result.status.text
	exit(1)

for r in result.resources:
	# multiple r.locations could come back for each endpoint network i/f (not implemented)
	# we just select the first for convenience
	print r.id.resource + " | " + r.id.sub_type + " | " + r.id.name + " | " + r.location[0]
