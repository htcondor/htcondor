#!/usr/bin/env python
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
from qmf.console import Session
from sys import exit, argv

gridclasses = [ 'master', 'grid', 'collector', 'negotiator', 'slot', 'scheduler', 'jobserver', 'submitter', 'submission' ]

url = len(argv) > 1 and argv[1] or "amqp://localhost:5672"
session = Session();
try:
	broker = session.addBroker(url)
except:
	print 'Unable to connect to broker'
	exit(1)

print session.getPackages()

for gridclass in gridclasses:
	qmfobjects = session.getObjects(_class=gridclass, _package='com.redhat.grid')
	for qmfobject in qmfobjects:
		print '\n'
		print gridclass, '=', qmfobject.getObjectId()
		print "*****Properties*****"
		for prop in qmfobject.getProperties():
			print " ",prop[0],"=",prop[1]
		print "*****Statistics*****"
		for stat in qmfobject.getStatistics():
			print " ",stat[0],"=",stat[1]
		print "*****Methods********"
		for meth in qmfobject.getMethods():
			print " ",meth
		if gridclass != 'collector':
			print qmfobject.echo(1,gridclass + ' test message')
		if gridclass == 'submission':
			print qmfobject.GetJobSummaries()

session.delBroker(broker)
