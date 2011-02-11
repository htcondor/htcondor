#!/usr/bin/env python

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
