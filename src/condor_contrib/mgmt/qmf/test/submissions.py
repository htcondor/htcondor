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

url = len(argv) > 2 and argv[2] or "amqp://localhost:5672"
session = Session();
try:
	broker = session.addBroker(url)
except:
	print 'Unable to connect to broker'
	exit(1)

try:
	agentName = "com.redhat.grid:%s" % argv[1]
	print "Connecting to agent at ", agentName
	agent = broker.getAgent(broker.getBrokerBank(),agentName)
	submissions = agent.getObjects(_class="submission", _package='com.redhat.grid')
except:
	print 'Unable to access agent or submissions'
	session.delBroker(broker)
	exit(1)

print "Current Submissions:"
for submission in submissions:
	print " ", submission.Name
	print "\tCompleted =\t%s" % submission.Completed
	print "\tHeld\t=\t%s" % submission.Held
	print "\tIdle\t=\t%s" % submission.Idle
	print "\tRemoved\t=\t%s" % submission.Removed
	print "\tRunning\t=\t%s" % submission.Running

session.delBroker(broker)
