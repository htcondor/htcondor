#!/usr/bin/python

import os
import re
import time
import errno
import signal
import socket
import classad
import unittest

class TestConfig(unittest.TestCase):

    def setUp(self):
        os.environ["_condor_FOO"] = "BAR"
        condor.reload_config()

    def test_config(self):
        self.assertEquals(condor.param["FOO"], "BAR")

    def test_reconfig(self):
        condor.param["FOO"] = "BAZ"
        self.assertEquals(condor.param["FOO"], "BAZ")
        os.environ["_condor_FOO"] = "1"
        condor.reload_config()
        self.assertEquals(condor.param["FOO"], "1")

class TestVersion(unittest.TestCase):

    def setUp(self):
        fd = os.popen("condor_version")
        self.lines = []
        for line in fd.readlines():
            self.lines.append(line.strip())
        if fd.close():
            raise RuntimeError("Unable to invoke condor_version")

    def test_version(self):
        self.assertEquals(condor.version(), self.lines[0])

    def test_platform(self):
        self.assertEquals(condor.platform(), self.lines[1])

def makedirs_ignore_exist(directory):
    try:
        os.makedirs(directory)
    except OSError, oe:
        if oe.errno != errno.EEXIST:
            raise

def remove_ignore_missing(file):
    try:
        os.unlink(file)
    except OSError, oe:
        if oe.errno != errno.ENOENT:
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
import condor

class TestWithDaemons(unittest.TestCase):

    def setUp(self):
        self.pid = -1
        os.environ["_condor_MASTER"] = os.path.join(os.getcwd(), "../../condor_master.V6/condor_master")
        os.environ["_condor_COLLECTOR"] = os.path.join(os.getcwd(), "../../condor_collector.V6/condor_collector")
        os.environ["_condor_SCHEDD"] = os.path.join(os.getcwd(), "../../condor_schedd.V6/condor_schedd")
        os.environ["_condor_PROCD"] = os.path.join(os.getcwd(), "../../condor_procd/condor_procd")
        os.environ["_condor_CONDOR_HOST"] = socket.getfqdn()
        os.environ["_condor_LOCAL_DIR"] = testdir
        os.environ["_condor_LOG"] =  '$(LOCAL_DIR)/log'
        os.environ["_condor_LOCK"] = '$(LOCAL_DIR)/lock'
        os.environ["_condor_RUN"] = '$(LOCAL_DIR)/run'
        os.environ["_condor_COLLECTOR_NAME"] = "python_classad_tests"
        os.environ["_condor_SCHEDD_NAME"] = "python_classad_tests"
        os.environ["_condor_MASTER_ADDRESS_FILE"] = "$(LOG)/.master_address"
        os.environ["_condor_COLLECTOR_ADDRESS_FILE"] = "$(LOG)/.collector_address"
        os.environ["_condor_SCHEDD_ADDRESS_FILE"] = "$(LOG)/.schedd_address"
        condor.reload_config()
        condor.SecMan().invalidateAllSessions()

    def launch_daemons(self, daemons=["MASTER", "COLLECTOR"]):
        makedirs_ignore_exist(condor.param["LOG"])
        makedirs_ignore_exist(condor.param["LOCK"])
        makedirs_ignore_exist(condor.param["EXECUTE"])
        makedirs_ignore_exist(condor.param["SPOOL"])
        makedirs_ignore_exist(condor.param["RUN"])
        remove_ignore_missing(condor.param["MASTER_ADDRESS_FILE"])
        remove_ignore_missing(condor.param["COLLECTOR_ADDRESS_FILE"])
        remove_ignore_missing(condor.param["SCHEDD_ADDRESS_FILE"])
        if "COLLECTOR" in daemons:
            os.environ["_condor_PORT"] = "9622"
            os.environ["_condor_COLLECTOR_ARGS"] = "-port $(PORT)"
            os.environ["_condor_COLLECTOR_HOST"] = "$(CONDOR_HOST):$(PORT)"
        if 'MASTER' not in daemons:
            daemons.append('MASTER')
        os.environ["_condor_DAEMON_LIST"] = ", ".join(daemons)
        condor.reload_config()
        self.pid = os.fork()
        if not self.pid:
            try:
                try:
                    os.execvp("condor_master", ["condor_master", "-f"])
                except Exception, e:
                    print str(e)
            finally:
                os._exit(1)
        for daemon in daemons:
            self.waitLocalDaemon(daemon)

    def tearDown(self):
        if self.pid > 1:
            os.kill(self.pid, signal.SIGQUIT)
            pid, exit_status = os.waitpid(self.pid, 0)
            self.assertTrue(os.WIFEXITED(exit_status))
            code = os.WEXITSTATUS(exit_status)
            self.assertEquals(code, 0)

    def waitLocalDaemon(self, daemon, timeout=5):
        address_file = condor.param[daemon + "_ADDRESS_FILE"]
        for i in range(timeout):
            if os.path.exists(address_file):
                return
            time.sleep(1)
        if not os.path.exists(address_file):
            raise RuntimeError("Waiting for daemon %s timed out." % daemon)

    def waitRemoteDaemon(self, dtype, dname, pool=None, timeout=5):
        if pool:
            coll = condor.Collector(pool)
        else:
            coll = condor.Collector()
        for i in range(timeout):
            try:
                return coll.locate(dtype, dname)
            except Exception:
                pass
            time.sleep(1)
        return coll.locate(dtype, dname)

    def testDaemon(self):
        self.launch_daemons(["COLLECTOR"])

    def testLocate(self):
        self.launch_daemons(["COLLECTOR"])
        coll = condor.Collector()
        coll_ad = coll.locate(condor.DaemonTypes.Collector)
        self.assertTrue("MyAddress" in coll_ad)
        self.assertEquals(coll_ad["Name"].split(":")[-1], os.environ["_condor_PORT"])

    def testRemoteLocate(self):
        self.launch_daemons(["COLLECTOR"])
        coll = condor.Collector()
        coll_ad = coll.locate(condor.DaemonTypes.Collector)
        remote_ad = self.waitRemoteDaemon(condor.DaemonTypes.Collector, "%s@%s" % (condor.param["COLLECTOR_NAME"], condor.param["CONDOR_HOST"]))
        self.assertEquals(remote_ad["MyAddress"], coll_ad["MyAddress"])

    def testScheddLocate(self):
        self.launch_daemons(["SCHEDD", "COLLECTOR"])
        coll = condor.Collector()
        name = "%s@%s" % (condor.param["SCHEDD_NAME"], condor.param["CONDOR_HOST"])
        schedd_ad = self.waitRemoteDaemon(condor.DaemonTypes.Schedd, name, timeout=10)
        self.assertEquals(schedd_ad["Name"], name)

    def testCollectorAdvertise(self):
        self.launch_daemons(["COLLECTOR"])
        coll = condor.Collector()
        now = time.time()
        ad = classad.ClassAd('[MyType="GenericAd"; Name="Foo"; Foo=1; Bar=%f; Baz="foo"]' % now) 
        coll.advertise([ad])
        for i in range(5):
            ads = coll.query(condor.AdTypes.Any, 'Name =?= "Foo"', ["Bar"])
            if ads: break
            time.sleep(1)
        self.assertEquals(len(ads), 1)
        self.assertEquals(ads[0]["Bar"], now)
        self.assertTrue("Foo" not in ads[0])

    def testScheddSubmit(self):
        self.launch_daemons(["SCHEDD", "COLLECTOR"])
        schedd = condor.Schedd()
        ad = classad.parse(open("tests/submit.ad"))
        schedd.submit(ad)

if __name__ == '__main__':
    unittest.main()

