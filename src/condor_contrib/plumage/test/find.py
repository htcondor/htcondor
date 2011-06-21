#!/usr/bin/env python

import pymongo
from sys import exit, argv

try:
    connection = pymongo.Connection("localhost")
    db = connection.condor
except Exception, e:
        print e
        exit(1)

for item in db['negotiator'].find().sort("MonitorSelfCPUUsage", pymongo.ASCENDING):
    print 'MonitorSelfCPUUsage->', item["MonitorSelfCPUUsage"], ' Machine->', item["Machine"]

