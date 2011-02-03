#!/usr/bin/env python

from qmf.console import Session
from sys import exit, argv

url = len(argv) > 1 and argv[1] or "amqp://localhost:5672"
session = Session();
try:
	broker = session.addBroker(url)
except:
	print 'Unable to connect to broker'
	exit(1)

submitters = session.getObjects(_class="submitter", _package='com.redhat.grid')
print "Current Submitters:"
for submitter in submitters:
	print submitter.Name
	print "\tRunning jobs = ", submitter.RunningJobs
	print "\tIdle jobs = ", submitter.IdleJobs
	print "\tHeld jobs = ", submitter.HeldJobs,"\n"

	# get the scheduler ref
	schedulers = session.getObjects(_objectId=submitter.schedulerRef)
	for scheduler in schedulers:
		print "\tscheduler is:", scheduler.Name
		print "\tTotal running jobs = ", scheduler.TotalRunningJobs
		print "\tTotal idle jobs = ", scheduler.TotalIdleJobs
		print "\tTotal held jobs = ", scheduler.TotalHeldJobs
		print "\tTotal removed jobs = ", scheduler.TotalRemovedJobs
		print "\tTotal job ads = ", scheduler.TotalJobAds

session.delBroker(broker)
