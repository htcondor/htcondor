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
from qmf.console import Session
from sys import exit, argv
import time, pwd, os

uid = pwd.getpwuid(os.getuid())[0]
if not uid:
    uid = "condor"

__annotations__ = {"Requirements": "com.redhat.grid.Expression"}
ad = {"Cmd": 		"/bin/sleep",
      "Args": 		"60",
      "Requirements": 	"TRUE",
      "Iwd": 		"/tmp",
      "Owner":		uid,
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
	# ensure that the value of sasl mechanisms is installed and
	# configured in /etc/sasl2/qpidd.conf
	broker = session.addBroker(url,mechanisms='PLAIN')
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
