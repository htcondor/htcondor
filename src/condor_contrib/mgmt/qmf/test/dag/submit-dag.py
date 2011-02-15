#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os, time
from qmf.console import Session
from sys import exit, argv
from subprocess import Popen, PIPE

exe_file = Popen('which condor_dagman', shell=True, stdout=PIPE).stdout.readline().rstrip('\n')
pwd_dir = Popen('pwd', shell=True, stdout=PIPE).stdout.readline().rstrip('\n')
cfg_file = Popen('echo $CONDOR_CONFIG', shell=True, stdout=PIPE).stdout.readline().rstrip('\n')

dag_file = len(argv) > 1 and argv[1]  or "diamond.dag"
url = len(argv) > 2 and argv[2] or "amqp://localhost:5672"

UNIVERSE = {"VANILLA": 5, "SCHEDULER": 7, "GRID": 9, "JAVA": 10, "PARALLEL": 11, "LOCAL": 12, "VM": 13}
__annotations__ = {
	"Requirements": "com.redhat.grid.Expression", 
	"OnExitRemove": "com.redhat.grid.Expression",
	"CopyToSpool": "com.redhat.grid.Expression"
	}

ad = dict()
ad["!!descriptors"] = __annotations__
ad["Cmd"] = exe_file
ad["Args"] = "-f -Debug 3 -allowversionmismatch -usedagdir -Lockfile  %s/%s.lock -Dag  %s/%s" % (pwd_dir,dag_file,pwd_dir,dag_file)
ad["JobUniverse"] = UNIVERSE["SCHEDULER"]
ad["Iwd"] = pwd_dir
ad["Owner"] = "pmackinn"
ad["Out"] = "%s/%s.out" % (pwd_dir,dag_file)
ad["Err"] = "%s/%s.err" % (pwd_dir,dag_file)
ad["UserLog"] = "%s/%s.log" % (pwd_dir,dag_file)
ad["Requirements"] = "True"
ad["RemoveKillSig"] = "SIGUSR1"
ad["OnExitRemove"] = "ExitSignal =?= 11 || (ExitCode =!= UNDEFINED && ExitCode >= 0 && ExitCode <= 2)"
ad["CopyToSpool"] = "False"

vars = list()
vars.append("_CONDOR_DAGMAN_LOG=%s.out" % dag_file)
vars.append("_CONDOR_MAX_DAGMAN_LOG=0")

# do this in case script is run in dev env
for param in os.environ.keys():
	if ("CONDOR" in param) or ("PATH" in param):
		vars.append("%s=%s" % (param,os.environ[param]))
ad["Environment"] = " ".join(vars)

for attr in sorted(ad):
	print attr,"=",ad[attr]

session = Session(); broker = session.addBroker(url)
schedulers = session.getObjects(_class="scheduler", _package="com.redhat.grid")
result = schedulers[0].SubmitJob(ad)

if result.status:
    print "Error submitting:", result.text
    session.delBroker(broker)
    exit(1)

print "Submitted job:", result.Id
time.sleep(5)

submissions = session.getObjects(_class="submission", _package="com.redhat.grid")
submission = reduce(lambda x,y: x or (y.Name == result.Id and y), submissions, False)
if not submission:
    print "Did not find submission... is the schedd or jobserver publishing submissions?"
    session.delBroker(broker)
    exit(1)

print "Properties:"
for prop in submission.getProperties():
    print " ",prop[0],"=",prop[1]

session.delBroker(broker)
