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
import argparse

parser = argparse.ArgumentParser(description='Query Condor ODS for statistics')
parser.add_argument('-v','--verbose', action="store_true",default=False, help='enable logging')
parser.add_argument('-s','--server', action="store", nargs='?', dest='server',
                    default='localhost',
                    help='mongodb database server location: e.g., somehost, localhost:2011')
parser.add_argument('-ul','--userlist', action="store_true",default=False, help='list all submitters')
parser.add_argument('-ugl','--usergrouplist', action="store_true",default=False, help='list all submitter groups')
parser.add_argument('-rl','--resourcelist', action="store_true",default=False, help='list all resource')
parser.add_argument('-rgl','--resourcegrouplist', action="store_true",default=False, help='list all resources groups')
args =  parser.parse_args()

try:
	connection = pymongo.Connection(args.server)
	db = connection.condor_stats
except Exception, e:
		print e
		exit(1)

if args.userlist:
	for user in db['samples.submitter'].distinct('sn'):
		print user
elif args.usergrouplist:
	# TODO: revisit with mongodb group()
	mnlist = db['samples.submitter'].distinct('mn')
	for mn in mnlist:
		snlist = db['samples.submitter'].find({'mn': mn}).distinct('sn')
		for sn in snlist:
			print '%s/%s' % (sn,mn)
elif args.resourcelist:
	for item in db['samples.machine'].distinct('mn'):
		print item
else:
	parser.print_help()

#print "Submitter\t\tTimestamp\t\t\tRunning\tHeld\tIdle"
#print "---------\t\t---------\t\t\t-------\t----\t----"
#for item in db['samples.submitter'].find().sort("sn", pymongo.ASCENDING):
	#print item['sn'],"\t",item['ts'], "\t",item['jr'],"\t",item['jh'],"\t",item['ji']

##for item in db['samples.machine'].find().sort("mn", pymongo.ASCENDING):
	##print item['mn']," ",item['ts'], " ", "%s/%s" % (item['ar'],item['os'])
