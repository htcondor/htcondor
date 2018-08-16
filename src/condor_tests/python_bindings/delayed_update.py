#!/usr/bin/python

import os
import sys
import optparse

print "Invoked as %s" % " ".join(sys.argv)
parser = optparse.OptionParser()
parser.add_option("--prefix", dest="prefix", default="Chirp")
parser.add_option("--type", dest="type", default="delayed")
parser.add_option("--shouldwork", dest="shouldwork", default="false")
opts = parser.parse_args()[0]
if opts.shouldwork.lower() == "true":
    opts.shouldwork = True
else:
    opts.shouldwork = False

os.system("condor_config_val -dump")

def sendDelayedUpdate(attr, val):
    print "Invoking: condor_chirp set_job_attr_delayed %s %s" % (attr, val)
    fd = os.popen("condor_chirp set_job_attr_delayed %s %s" % (attr, val))
    sys.stdout.write(fd.read())
    return fd.close()

if opts.type == "delayed":
    if opts.shouldwork and (opts.prefix.lower() != os.environ.get("_CHIRP_DELAYED_UPDATE_PREFIX").lower()[:-1]):
        print "Invalid environment: %s" % os.environ.get("_CHIRP_DELAYED_UPDATE_PREFIX")
        print "FAILED"
        sys.exit(0)
    result = sendDelayedUpdate("%sFoo" % opts.prefix, "2")
    success = (result != opts.shouldwork)
    if success:
        print "SUCCESS"
        sys.exit(0)
elif opts.type == "delayeddos":
    if sendDelayedUpdate("ChirpFoo", '\\"%s\\"' % ("0"*990)):
        print "FAILED"
        sys.exit(1)
    if not sendDelayedUpdate("ChirpBar", '"%s"' % ("0"*1100)):
        print "FAILED"
        sys.exit(1)
    for i in range(1, 50):
        if sendDelayedUpdate("ChirpFoo%d" % i, "2"):
            print "FAILED"
            sys.exit(1)
    for i in range(1, 50):
        if sendDelayedUpdate("ChirpFoo%d" % i, "1"):
            print "FAILED"
            sys.exit(1)
    if not sendDelayedUpdate("ChirpFoo50", "1"):
        print "FAILED"
        sys.exit(1)
    print "SUCCESS"
    sys.exit(0)
elif opts.type == "io":
    print "Invoking: condor_chirp fetch tests_tmp/test_chirp_io test_chirp_io"
    result = os.system("condor_chirp fetch tests_tmp/test_chirp_io test_chirp_io")
    print result
    if (opts.shouldwork and (result!=0)) or (not opts.shouldwork and (result==0)):
        print "FAILED - incorrect condor_chirp result"
        sys.exit(1)
    if opts.shouldwork and open("test_chirp_io").read() != "hello world":
        print "FAILED - incorrect test_chirp_io contents"
        sys.exit(1)
    print "SUCCESS"
    sys.exit(0)
elif opts.type == "update":
    print "Invoking: condor_chirp set_job_attr %sFoo 2" % opts.prefix
    result = os.system("condor_chirp set_job_attr %sFoo 2" % opts.prefix)
    print result
    if (opts.shouldwork and (result!=0)) or (not opts.shouldwork and (result==0)):
        print "FAILED - incorrect condor_chirp result"
        sys.exit(1)
    print "SUCCESS"
    sys.exit(0)
else:
    print "Invalid test type specified!"

print "FAILED"
sys.exit(1)

