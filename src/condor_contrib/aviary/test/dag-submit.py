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

def attr_builder(type_, format):
    def attr(name, value):
        attr = client.factory.create("ns0:Attribute")
        attr.name = name
        attr.type = type_
        attr.value = format % (value,)
        return attr
        return attr

string_attr=attr_builder('STRING', '%s')
int_attr=attr_builder('INTEGER', '%d')
expr_attr=attr_builder('EXPRESSION', '%s')

parser = argparse.ArgumentParser(description='Submit a DAG job remotely via SOAP.')
parser.add_argument('-v', '--verbose', action='store_true',default=False, help='enable SOAP logging')
parser.add_argument('-u', '--url', action='store', nargs='?', dest='url',
    default='http://localhost:9090/services/job/submitJob',
    help='http or https URL prefix to be added to cmd')
parser.add_argument('dag', action='store', help='full path to dag file')
args =  parser.parse_args()

uid = pwd.getpwuid(os.getuid())[0] or "nobody"

# TODO: fix this for http
client = Client('file:/var/lib/condor/aviary/services/job/aviary-job.wsdl')

client.set_options(location=args.url)

if args.verbose:
    logging.basicConfig(level=logging.INFO)
    logging.getLogger('suds.client').setLevel(logging.DEBUG)
    print client

try:
result = client.service.submitJob(
    '/usr/bin/condor_dagman',
    '-f -l . -Debug 3 -AutoRescue 1 -DoRescueFrom 0 -Allowversionmismatch -Lockfile %s.lock -Dag %s' % (args.dag, args.dag),
    uid,
    args.dag[:args.dag.rindex('/')],
    args.dag[args.dag.rindex('/')+1:],
    [],
    [string_attr('Env', '_CONDOR_MAX_DAGMAN_LOG=0;_CONDOR_DAGMAN_LOG=%s.dagman.out' % (args.dag,)),
    int_attr('JobUniverse', 7),
    string_attr('UserLog', args.dag + '.dagman.log'),
    string_attr('RemoveKillSig', 'SIGUSR1'),
    expr_attr('OnExitRemove', '(ExitSignal =?= 11 || (ExitCode =!= UNDEFINED && ExitCode >= 0 && ExitCode <= 2))')]
    )
except Exception, e:
    print 'invocation failed at: ', args.url
    print e
    sys.exit(1) 

if result.status.code != 'OK':
    print result.status.code,'; ', result.status.text
    sys.exit(1)

print args.verbose and result or result.id.job
