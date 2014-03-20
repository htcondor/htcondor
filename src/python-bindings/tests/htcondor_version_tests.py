#!/usr/bin/python

import os
import sys
import errno
import unittest

class TestConfig(unittest.TestCase):

    def setUp(self):
        os.environ["_condor_FOO"] = "BAR"
        htcondor.reload_config()

    def test_config(self):
        self.assertEquals(htcondor.param["FOO"], "BAR")

    def test_reconfig(self):
        htcondor.param["FOO"] = "BAZ"
        self.assertEquals(htcondor.param["FOO"], "BAZ")
        os.environ["_condor_FOO"] = "1"
        htcondor.reload_config()
        self.assertEquals(htcondor.param["FOO"], "1")


class TestVersion(unittest.TestCase):

    def setUp(self):
        fd = os.popen("condor_version")
        self.lines = []
        for line in fd.readlines():
            self.lines.append(line.strip())
        if fd.close():
            raise RuntimeError("Unable to invoke condor_version")

    def test_version(self):
        self.assertEquals(htcondor.version(), self.lines[0])

    def test_platform(self):
        self.assertEquals(htcondor.platform(), self.lines[1])

def makedirs_ignore_exist(directory):
    try:
        os.makedirs(directory)
    except:
        exctype, oe = sys.exc_info()[:2]
        if not issubclass(exctype, OSError): raise
        if oe.errno != errno.EEXIST:
            raise

# Bootstrap condor
testdir = os.path.join(os.getcwd(), "tests_tmp")
logdir = os.path.join(testdir, "log")
makedirs_ignore_exist(testdir)
makedirs_ignore_exist(logdir)
config_file = os.path.join(testdir, "condor_config")
open(config_file, "w").close()
os.environ["CONDOR_CONFIG"] = config_file
os.environ["_condor_TOOL_LOG"] = os.path.join(logdir, "ToolLog")
import htcondor

if __name__ == '__main__':
    unittest.main()

