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

# uses pymongo - http://pypi.python.org/pypi/pymongo/
import pymongo
from datetime import timedelta, datetime
from sys import exit, argv
import time, pwd, os
import logging
from optparse import OptionParser
from dateutil import *
from dateutil.parser import *

def print_user(user,start,end):
	for item in db['samples.submitter'].find({"sn":{'$regex':'^'+user}, 'ts':{'$gte': parse(start), '$lt': parse(end)}}):
		print item['sn'],"\t",item['ts'], "\t",item['jr'],"\t",item['jh'],"\t",item['ji']

def print_resource(resource,start,end):
	for item in db['samples.machine'].find({"mn":{'$regex':resource}, 'ts':{'$gte': parse(start), '$lt': parse(end)}}):
		print item['mn'],"\t",item['ts'], "\t","%s/%s" % (item['ar'],item['os']),"\t",item['ki'],"\t",item['la']

def print_users():
		for user in db['samples.submitter'].distinct('sn'):
			print user

def print_usergroups():
	# TODO: revisit with mongodb group()
	mnlist = db['samples.submitter'].distinct('mn')
	for mn in mnlist:
		snlist = db['samples.submitter'].find({'mn': mn}).distinct('sn')
		for sn in snlist:
			print '%s/%s' % (sn,mn)

def print_resourcelist():
	for item in db['samples.machine'].distinct('mn'):
		print item

parser = OptionParser(description='Query Condor ODS for statistics')
parser.add_option('-v','--verbose', action="store_true",default=False, help='enable logging')
parser.add_option('-s','--server', action="store", nargs='?', dest='server',
                    default='localhost',
                    help='mongodb database server location: e.g., somehost, localhost:2011')
parser.add_option('--u','--user', dest="user", help='stats for a single submitter')
parser.add_option('--r','--resource', dest="resource", help='stats for a single resource')
parser.add_option('--f','--from', dest="start", help='records from datetime', default=str(datetime.now()-timedelta(hours=1)))
parser.add_option('--t','--to', dest="end", help='records to datetime',default=str(datetime.now()))
parser.add_option('--ul','--userlist', action="store_true",dest="userlist", default=False, help='list all submitters')
parser.add_option('--ugl','--usergrouplist', action="store_true",dest='usergrouplist',default=False, help='list all submitter groups')
parser.add_option('--rl','--resourcelist', action="store_true",dest='resourcelist',default=False, help='list all resources')
(options, args) =  parser.parse_args()

try:
	connection = pymongo.Connection(options.server)
	db = connection.condor_stats
except Exception, e:
	print e
	exit(1)

if options.user:
	print_user(options.user,options.start,options.end)
elif options.resource:
	print_resource(options.resource,options.start,options.end)
elif options.userlist:
	print_users()
elif options.usergrouplist:
	print_usergroups()
elif options.resourcelist:
	print_resourcelist()
else:
	parser.print_help()
