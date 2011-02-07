#!/usr/bin/env python
# -*- coding: utf-8 -*-

from qmf.console import Session
from sys import exit, argv, stdin
import time

cmds = ['SetJobAttribute', 'HoldJob', 'ReleaseJob', 'RemoveJob']

cmdarg = len(argv) > 1 and argv[1]
jobid =  len(argv) > 2 and argv[2]
url = len(argv) > 3 and argv[3] or "amqp://localhost:5672"

if cmdarg not in cmds:
	print "error unknown command: ", cmdarg
	print "available commands are: ",cmds
	exit(1)

try:
	session = Session();
	broker = session.addBroker(url)
	schedulers = session.getObjects(_class="scheduler", _package="com.redhat.grid")
except Exception, e:
	print "unable to access broker or scheduler"
	exit(1)

result = schedulers[0]._invoke(cmdarg,[jobid,"test"],[None])

if result.status:
	print "error invoking: ", cmdarg
	print result.text
	session.delBroker(broker)
	exit(1)

session.delBroker(broker)
