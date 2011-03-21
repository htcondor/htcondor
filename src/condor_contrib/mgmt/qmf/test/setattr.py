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
from sys import exit, argv, stdin
import time

jobid =  len(argv) > 1 and argv[1]
attr_name = len(argv) > 2 and argv[2]
attr_value = len(argv) > 3 and argv[3]
url = len(argv) > 4 and argv[4] or "amqp://localhost:5672"

try:
	session = Session();
	broker = session.addBroker(url)
	schedulers = session.getObjects(_class="scheduler", _package="com.redhat.grid")
except Exception, e:
	print "unable to access broker or scheduler"
	print e
	exit(1)

result = schedulers[0].SetJobAttribute(jobid, attr_name, attr_value)

if result.status:
	print result.text
	session.delBroker(broker)
	exit(1)

session.delBroker(broker)
