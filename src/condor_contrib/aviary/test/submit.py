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
import time, pwd, os, logging
from aviary.util import *

plugins = []
# NOTE: Suds has had little support for adding attributes
# to the request body until 0.4.1
# uncomment the following to enable the allowOverrides attribute

#from suds.plugin import MessagePlugin
#from suds.sax.attribute import Attribute
#class OverridesPlugin(MessagePlugin):
    #def marshalled(self, context):
        #sj_body = context.envelope.getChild('Body')[0]
        #sj_body.attributes.append(Attribute("allowOverrides", "true"))
#plugins=[OverridesPlugin()]

uid = pwd.getpwuid(os.getuid())[0]
if not uid:
    uid = "condor"

# change these for other default locations and ports
wsdl = 'file:/var/lib/condor/aviary/services/job/aviary-job.wsdl'

parser = build_basic_parser('Submit a sample job remotely via SOAP.','http://localhost:39090/services/job/submitJob')
(opts,args) =  parser.parse_args()

client = create_suds_client(opts,wsdl,plugins)
client.set_options(location=opts.url)

# add specific requirements here
req1 = client.factory.create("ns0:ResourceConstraint")
req1.type = 'OS'
req1.value = 'LINUX'
reqs = [ req1 ]

# add extra HTCondor-specific or custom job attributes here
extra1 = client.factory.create("ns0:Attribute")
extra1.name = 'RECIPE'
extra1.type = 'STRING'
extra1.value = '"SECRET_SAUCE"'
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
	print "invocation failed at: ", opts.url
	print e
	exit(1)	

if result.status.code != "OK":
	print result.status.code,"; ", result.status.text
	exit(1)

print opts.verbose and result or result.id.job
