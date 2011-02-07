#!/usr/bin/env python
# -*- coding: utf-8 -*-

from qmf.console import Session
from sys import exit, argv
import time

__annotations__ = {"Requirements": "com.redhat.grid.Expression"}
ad = {"Cmd": 		"/bin/sleep",
      "Args": 		"60",
      "Requirements": 	"TRUE",
      "Iwd": 		"/tmp",
      "Owner":		"nobody",
      "!!descriptors":	__annotations__
      }

quiet = False
url = "amqp://localhost:5672"

for arg in argv[1:]:
	if arg == '-q':
		quiet = True
	if "amqp://" in arg:
		url = arg

if not quiet:
	print ad;

try:
	session = Session();
	broker = session.addBroker(url)
except Exception, e:
	print "unable to access broker or scheduler"
	print e
	exit(1)

schedulers = session.getObjects(_class="scheduler", _package="com.redhat.grid")
result = schedulers[0].SubmitJob(ad)

if result.status:
	print "Error submitting:", result.text
	session.delBroker(broker)
	exit(1)

if not quiet:
	print "Submitted:", result.Id
	print "waiting for Submission to propagate..."
time.sleep(10)

# chop up our id
submission_part = result.Id.rsplit('.',1)[0]
job_part = result.Id.rsplit('#',1)[1]

submissions = session.getObjects(_class="submission", _package="com.redhat.grid")
submission = reduce(lambda x,y: x or (y.Name == submission_part and y), submissions, False)
if not submission:
    print "Did not find submission... is the schedd or jobserver publishing submissions?"
    session.delBroker(broker)
    exit(1)

if not quiet:
	print "Properties:"
	for prop in submission.getProperties():
		print " ",prop[0],"=",prop[1]

# just emit the cluster.proc for consumption by
# other API test scripts
print job_part

session.delBroker(broker)
