#!/usr/bin/python

import os
import re
import time
import errno
import signal
import socket
import classad
import unittest

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
import htcondor

class TestWithDaemons(unittest.TestCase):

    def setUp(self):
        self.pid = -1
        os.environ["_condor_MASTER"] = os.path.join(os.getcwd(), "../../condor_master.V6/condor_master")
        os.environ["_condor_COLLECTOR"] = os.path.join(os.getcwd(), "../../condor_collector.V6/condor_collector")
        os.environ["_condor_SCHEDD"] = os.path.join(os.getcwd(), "../../condor_schedd.V6/condor_schedd")
        os.environ["_condor_PROCD"] = os.path.join(os.getcwd(), "../../condor_procd/condor_procd")
        os.environ["_condor_STARTD"] = os.path.join(os.getcwd(), "../../condor_startd.V6/condor_startd")
        os.environ["_condor_STARTER"] = os.path.join(os.getcwd(), "../../condor_starter.V6.1/condor_starter")
        os.environ["_condor_NEGOTIATOR"] = os.path.join(os.getcwd(), "../../condor_negotiator.V6/condor_negotiator")
        os.environ["_condor_SHADOW"] = os.path.join(os.getcwd(), "../../condor_shadow.V6.1/condor_shadow")
        os.environ["_condor_STARTER.PLUGINS"] = os.path.join(os.getcwd(), "../lark/lark-plugin.so")
        os.environ["_condor_USE_NETWORK_NAMESPACES"] = "TRUE"
        #os.environ["_condor_LARK_NETWORK_ACCOUNTING"] = "TRUE"
        #now make the default configuration to be "bridge"
        #os.environ["_condor_STARTD_ATTRS"] = "LarkNetworkType, LarkAddressType, LarkBridgeDevice"
        #os.environ["_condor_LarkNetworkType"] = "bridge"
        #os.environ["_condor_LarkNetBridgeDevice"] = "eth0"
        #os.environ["_condor_LarkAddressType"] = "dhcp"

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
        os.environ["_condor_STARTD_ADDRESS_FILE"] = "$(LOG)/.startd_address"
        os.environ["_condor_NEGOTIATOR_ADDRESS_FILE"] = "$(LOG)/.negotiator_address"
        # Various required attributes for the startd
        os.environ["_condor_START"] = "TRUE"
        os.environ["_condor_SUSPEND"] = "FALSE"
        os.environ["_condor_CONTINUE"] = "TRUE"
        os.environ["_condor_PREEMPT"] = "FALSE"
        os.environ["_condor_KILL"] = "FALSE"
        os.environ["_condor_WANT_SUSPEND"] = "FALSE"
        os.environ["_condor_WANT_VACATE"] = "FALSE"
        # Remember to check the correctness of network policy script path defined
        os.environ["_condor_STARTER_NETWORK_POLICY_SCRIPT_PATH"] = os.path.join(os.getcwd(), "../lark/LarkNetworkPolicy/lark_network_policy.py")
        os.environ["_condor_STARTER_DEBUG"] = "D_FULLDEBUG"
        htcondor.reload_config()
        htcondor.SecMan().invalidateAllSessions()

    def launch_daemons(self, daemons=["MASTER", "COLLECTOR"]):
        makedirs_ignore_exist(htcondor.param["LOG"])
        makedirs_ignore_exist(htcondor.param["LOCK"])
        makedirs_ignore_exist(htcondor.param["EXECUTE"])
        makedirs_ignore_exist(htcondor.param["SPOOL"])
        makedirs_ignore_exist(htcondor.param["RUN"])
        remove_ignore_missing(htcondor.param["MASTER_ADDRESS_FILE"])
        remove_ignore_missing(htcondor.param["COLLECTOR_ADDRESS_FILE"])
        remove_ignore_missing(htcondor.param["SCHEDD_ADDRESS_FILE"])
        if "COLLECTOR" in daemons:
            os.environ["_condor_PORT"] = "9622"
            os.environ["_condor_COLLECTOR_ARGS"] = "-port $(PORT)"
            os.environ["_condor_COLLECTOR_HOST"] = "$(CONDOR_HOST):$(PORT)"
        if 'MASTER' not in daemons:
            daemons.append('MASTER')
        os.environ["_condor_DAEMON_LIST"] = ", ".join(daemons)
        htcondor.reload_config()
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
        address_file = htcondor.param[daemon + "_ADDRESS_FILE"]
        for i in range(timeout):
            if os.path.exists(address_file):
                return
            time.sleep(1)
        if not os.path.exists(address_file):
            raise RuntimeError("Waiting for daemon %s timed out." % daemon)

    def waitRemoteDaemon(self, dtype, dname, pool=None, timeout=5):
        if pool:
            coll = htcondor.Collector(pool)
        else:
            coll = htcondor.Collector()
        for i in range(timeout):
            try:
                return coll.locate(dtype, dname)
            except Exception:
                pass
            time.sleep(1)
        return coll.locate(dtype, dname)

    def testNetworkPolicyNAT(self):
        jobqueue_log_dir = os.path.join(os.getcwd(), "tests_tmp/spool")
        filelist = [f for f in os.listdir(jobqueue_log_dir) if f.startswith("job_queue.log")]
        for f in filelist:
            os.remove(os.path.join(jobqueue_log_dir, f))
        self.launch_daemons(["SCHEDD", "COLLECTOR", "STARTD", "NEGOTIATOR"])
        output_file = os.path.join(testdir, "lark_test_1.out")
        if os.path.exists(output_file):
            os.unlink(output_file)
        schedd = htcondor.Schedd()
        coll = htcondor.Collector()
        ad = classad.parse(open("tests/lark_submit_1.ad"))
        ads = []
        cluster = schedd.submit(ad, 1, False, ads)
        #print ads[0]
        for i in range(60):
            #ads = coll.query(htcondor.AdTypes.Startd, "true", ["LarkNetworkType"])
            #ads = coll.query("true", ["LarkNetworkType"])
            ads = schedd.query("ClusterId == %d" % cluster, ["LarkNetworkType"])
            print ads
            if len(ads) != 0:
                if "LarkNetworkType" in ads[0].keys():
                    break
            time.sleep(1)
        #machine_ad = classad.parseOld(open(output_file, "r"))
        self.assertTrue(len(ads) == 1)
        self.assertTrue("LarkNetworkType" in ads[0].keys())
        self.assertEquals(job_ad["LarkNetworkType"], "nat")

    def testNetworkAccounting(self):
        jobqueue_log_dir = os.path.join(os.getcwd(), "tests_tmp/spool")
        filelist = [f for f in os.listdir(jobqueue_log_dir) if f.startswith("job_queue.log")]
        for f in filelist:
            print "Removing", f
            os.remove(os.path.join(jobqueue_log_dir, f))
        self.launch_daemons(["SCHEDD", "COLLECTOR", "STARTD", "NEGOTIATOR"])
        output_file = os.path.join(testdir, "lark_test_2.out")
        if os.path.exists(output_file):
            os.unlink(output_file)
        schedd = htcondor.Schedd()
        ad = classad.parse(open("tests/lark_submit_2.ad"))
        ads = []
        cluster = schedd.submit(ad, 1, False, ads)
        for i in range(60):
            ads = schedd.query("ClusterId == %d" % cluster, ["JobStatus"])
            job_ad = ads[0]
            if job_ad["JobStatus"] == 4:
                break
            if i % 2 == 0:
                schedd.reschedule()
            time.sleep(1)
        ads = schedd.query("ClusterId == %d" % cluster, [])
        print ads[0]
        self.assertTrue("NetworkIncoming" in ads[0].keys() and ads[0]["NetworkIncoming"] > 0)

    def testNetworkPolicyBridge(self):
        jobqueue_log_dir = os.path.join(os.getcwd(), "tests_tmp/spool")
        filelist = [f for f in os.listdir(jobqueue_log_dir) if f.startswith("job_queue.log")]
        for f in filelist:
            print "Removing", f
            os.remove(os.path.join(jobqueue_log_dir, f))
        self.launch_daemons(["SCHEDD", "COLLECTOR", "STARTD", "NEGOTIATOR"])
        output_file = os.path.join(testdir, "lark_test_3.out")
        if os.path.exists(output_file):
            os.unlink(output_file)
        schedd = htcondor.Schedd()
        ad = classad.parse(open("tests/lark_submit_3.ad"))
        ads = []
        cluster = schedd.submit(ad, 1, False, ads)
        for i in range(60):
            ads = schedd.query("ClusterId == %d" % cluster, ["LarkNetworkType"])
            print ads
            if len(ads) != 0:
                if "LarkNetworkType" in ads[0].keys():
                    break
            time.sleep(1)
        self.assertTrue(len(ads) == 1)
        self.assertTrue("LarkNetworkType" in ads[0].keys())
        self.assertEquals(job_ad["LarkNetworkType"], "bridge")

if __name__ == '__main__':
    unittest.main()

