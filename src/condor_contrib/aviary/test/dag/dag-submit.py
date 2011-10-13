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
import sys, pwd, os, logging, argparse
from subprocess import Popen, PIPE

# closure to build out our various attribute types
def attr_builder(type_, format):
	def make_attr(name, value):
		attr = client.factory.create("ns0:Attribute")
		attr.name = name
		attr.type = type_
		attr.value = format % (value,)
		return attr
	return make_attr

string_attr = attr_builder('STRING', '%s')
int_attr = attr_builder('INTEGER', '%d')
expr_attr = attr_builder('EXPRESSION', '%s')

wsdl = 'file:/var/lib/condor/aviary/services/job/aviary-job.wsdl'
key = '/etc/pki/tls/certs/client.key'
cert = '/etc/pki/tls/certs/client.crt'
root = '/etc/pki/tls/certs/server.crt'

exe_file = Popen('which condor_dagman', shell=True, stdout=PIPE).stdout.readline().rstrip('\n')
pwd_dir = Popen('pwd', shell=True, stdout=PIPE).stdout.readline().rstrip('\n')
cfg_file = Popen('echo $CONDOR_CONFIG', shell=True, stdout=PIPE).stdout.readline().rstrip('\n')

UNIVERSE = {"VANILLA": 5, "SCHEDULER": 7, "GRID": 9, "JAVA": 10, "PARALLEL": 11, "LOCAL": 12, "VM": 13}

parser = argparse.ArgumentParser(description='Submit a DAG job remotely via SOAP.')
parser.add_argument('-v', '--verbose', action='store_true',default=False, help='enable SOAP logging')
parser.add_argument('-u', '--url', action='store', nargs='?', dest='url',
	default='http://localhost:9090/services/job/submitJob',
	help='http or https URL')
parser.add_argument('-k','--key', action="store", nargs='?', dest='key', help='client SSL key file')
parser.add_argument('-c','--cert', action="store", nargs='?', dest='cert', help='client SSL certificate file')
parser.add_argument('-r','--root', action="store", nargs='?', dest='root', help='server SSL certificate file')
parser.add_argument('-s','--server', action="store_true", default=False, dest='verify', help='enable server certificate verification')
parser.add_argument('dag', action='store', help='full path to dag file')
args =  parser.parse_args()

exe_args = '-f -l . -Debug 3 -AutoRescue 1 -DoRescueFrom 0 -Allowversionmismatch -Lockfile %s.lock -Dag %s' % (args.dag, args.dag)

uid = pwd.getpwuid(os.getuid())[0] or "nobody"

if "https://" in args.url:
	client = Client(wsdl,transport = HTTPSFullCertTransport(key,cert,root,args.verify))
else:
	client = Client(wsdl)

client.set_options(location=args.url)

# a dag submit is actually a general submit with LOTS of extra attributes
extras = []
extras.append(int_attr('JobUniverse', UNIVERSE["SCHEDULER"]))
extras.append(string_attr('UserLog', args.dag + '.dagman.log'))
extras.append(string_attr('RemoveKillSig', 'SIGUSR1'))
extras.append(expr_attr('OnExitRemove', '(ExitSignal =?= 11 || (ExitCode =!= UNDEFINED && ExitCode >= 0 && ExitCode <= 2))'))

# do this in case script is run in dev env
vars = []
for param in os.environ.keys():
	if ("CONDOR" in param) or ("PATH" in param):
		vars.append("%s=%s" % (param,os.environ[param]))
vars.append('_CONDOR_MAX_DAGMAN_LOG=0')
vars.append('_CONDOR_DAGMAN_LOG=%s.dagman.out' % (args.dag,))
extras.append(string_attr('Environment', " ".join(vars)))

if args.verbose:
	print vars
	print extras
	logging.basicConfig(level=logging.INFO)
	logging.getLogger('suds.client').setLevel(logging.DEBUG)
	print client

try:
	result = client.service.submitJob(
    exe_file,
    exe_args,
    uid,
    args.dag[:args.dag.rindex('/')],
    args.dag[args.dag.rindex('/')+1:],
    [],
    extras
    )
except Exception, e:
    print 'invocation failed at: ', args.url
    print e
    sys.exit(1) 

if result.status.code != 'OK':
    print result.status.code,'; ', result.status.text
    sys.exit(1)

print args.verbose and result or result.id.job
