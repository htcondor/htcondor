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
from suds.client import Client
import sys, pwd, os, logging
from optparse import OptionParser
from subprocess import Popen, PIPE
from aviary.https import *
from aviary.util import *

#NOTE: ensure this DAG script directory is writable by the user

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

wsdl = 'file:/var/lib/condor/aviary/services/job/aviary-job.wsdl'

exe_file = Popen('which condor_dagman', shell=True, stdout=PIPE).stdout.readline().rstrip('\n')
iwd = Popen('pwd', shell=True, stdout=PIPE).stdout.readline().rstrip('\n')
UNIVERSE = {"VANILLA": 5, "SCHEDULER": 7, "GRID": 9, "JAVA": 10, "PARALLEL": 11, "LOCAL": 12, "VM": 13}

parser = build_basic_parser('Submit a DAG job remotely via SOAP.','http://localhost:39090/services/job/submitJob')
parser.add_option('--d','--dag', action='store', dest='dag', help='full path to dag file')
(opts, args) =  parser.parse_args()

if opts.dag is None:
	print "Path of dag submit file must be supplied\n"
	parser.print_help()
	sys.exit(1)

dag = opts.dag
url = opts.url
verbose = opts.verbose

exe_args = '-f -l . -Debug 3 -AutoRescue 1 -DoRescueFrom 0 -Allowversionmismatch -Lockfile %s.lock -Dag %s' % (dag, dag)

uid = pwd.getpwuid(os.getuid())[0] or "nobody"

client = create_suds_client(opts,wsdl,plugins)
client.set_options(location=url)

# closure to build out our various attribute types
def attr_builder(type_, format):
	def make_attr(name, value):
		attr = client.factory.create('ns0:Attribute')
		attr.name = name
		attr.type = type_
		attr.value = format % (value,)
		return attr
	return make_attr

string_attr = attr_builder('STRING', '%s')
int_attr = attr_builder('INTEGER', '%d')
expr_attr = attr_builder('EXPRESSION', '%s')

# a dag submit is actually a general submit with specialized exe, args, and extra attributes
extras = []
extras.append(int_attr('JobUniverse', UNIVERSE["SCHEDULER"]))
extras.append(string_attr('UserLog', dag + '.dagman.log'))
extras.append(string_attr('RemoveKillSig', 'SIGUSR1'))
extras.append(expr_attr('OnExitRemove', '(ExitSignal =?= 11 || (ExitCode =!= UNDEFINED && ExitCode >= 0 && ExitCode <= 2))'))

# do this in case script is run in dev env
vars = []
for param in os.environ.keys():
	if ("CONDOR" in param) or ("PATH" in param):
		vars.append("%s=%s" % (param,os.environ[param]))
vars.append('_CONDOR_MAX_DAGMAN_LOG=0')
vars.append('_CONDOR_DAGMAN_LOG=%s.dagman.out' % (dag,))
extras.append(string_attr('Environment', " ".join(vars)))

# separate dag file from path for submit
(dir_name, file_name) = os.path.split(dag)

if verbose:
	print 'IWD=',dir_name
	print 'Dag file=',file_name
	print vars
	print extras

try:
	result = client.service.submitJob(
    exe_file,
    exe_args,
    uid,
    dir_name or iwd,
    file_name,
    [],
    extras
    )
except Exception, e:
    print 'invocation failed at: ', url
    print e
    sys.exit(1) 

if result.status.code != 'OK':
    print result.status.code,'; ', result.status.text
    sys.exit(1)

print verbose and result or result.id.job
