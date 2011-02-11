#!/usr/bin/env python
# -*- coding: utf-8 -*-

from qmf.console import Session
from sys import exit, argv
from subprocess import Popen, PIPE

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

submit_list = []
print "Collecting jobs from Submissions..."
for submission in submissions:
	try:
		print submission.Name
		summaries = submission.GetJobSummaries()
		for summary in summaries.outArgs['Jobs']:
			submit_list.append("%s.%s" % (summary['ClusterId'],summary['ProcId']))
	except:
		print "GetJobSummaries error for", submission.Name
		session.delBroker(broker)
		exit(1)

print "Found %d jobs in QMF" % len(submit_list)

chproc = Popen('condor_history -format "%s." ClusterId -format "%s," ProcId', shell=True, stdout=PIPE)
history = sorted(chproc.stdout.readline().split(','))
del history[0]

cqproc = Popen('condor_q -format "%s." ClusterId -format "%s," ProcId', shell=True, stdout=PIPE)
live = sorted(cqproc.stdout.readline().split(','))
del live[0]

alljobs = sorted(set(history+live))
print "Checking %d condor jobs " % len(alljobs)
for job in alljobs:
	if not job in submit_list:
		print "No record of ", job

if len(alljobs) == len(submit_list):
	print "All jobs in condor accounted for in QMF"
else:
	print "Mismatch - inventory check failed"

session.delBroker(broker)
