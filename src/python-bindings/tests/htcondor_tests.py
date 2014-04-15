#!/usr/bin/python

import os
import re
import sys
import time
import errno
import types
import atexit
import signal
import socket
import classad
import datetime
import unittest

master_pid = 0
def kill_master():
    if master_pid: os.kill(master_pid, signal.SIGTERM)
atexit.register(kill_master)


def makedirs_ignore_exist(directory):
    try:
        os.makedirs(directory)
    except:
        exctype, oe = sys.exc_info()[:2]
        if not issubclass(exctype, OSError): raise
        if oe.errno != errno.EEXIST:
            raise


def remove_ignore_missing(file):
    try:
        os.unlink(file)
    except:
        exctype, oe = sys.exc_info()[:2]
        if not issubclass(exctype, OSError): raise
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
os.environ["_condor_TOOL_DEBUG"] = "D_FULLDEBUG, D_NETWORK"
import htcondor
htcondor.enable_log()

class WithDaemons(unittest.TestCase):

    def setUp(self):
        self.pid = -1
        to_delete = [i for i in os.environ if i.lower().startswith("_condor_")]
        for key in to_delete: del os.environ[key]
        os.environ["_condor_MASTER"] = os.path.join(os.getcwd(), "../condor_master.V6/condor_master")
        os.environ["_condor_COLLECTOR"] = os.path.join(os.getcwd(), "../condor_collector.V6/condor_collector")
        os.environ["_condor_SCHEDD"] = os.path.join(os.getcwd(), "../condor_schedd.V6/condor_schedd")
        os.environ["_condor_PROCD"] = os.path.join(os.getcwd(), "../condor_procd/condor_procd")
        os.environ["_condor_STARTD"] = os.path.join(os.getcwd(), "../condor_startd.V6/condor_startd")
        os.environ["_condor_STARTER"] = os.path.join(os.getcwd(), "../condor_starter.V6.1/condor_starter")
        os.environ["_condor_NEGOTIATOR"] = os.path.join(os.getcwd(), "../condor_negotiator.V6/condor_negotiator")
        os.environ["_condor_SHADOW"] = os.path.join(os.getcwd(), "../condor_shadow.V6.1/condor_shadow")
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
        os.environ["_condor_STARTD_DEBUG"] = "D_FULLDEBUG"
        os.environ["_condor_STARTER_DEBUG"] = "D_FULLDEBUG"
        os.environ["_condor_SHADOW_DEBUG"] = "D_FULLDEBUG|D_MACHINE"
        os.environ["_condor_NEGOTIATOR_ADDRESS_FILE"] = "$(LOG)/.negotiator_address"
        os.environ["_condor_NEGOTIATOR_CYCLE_DELAY"] = "1"
        os.environ["_condor_NEGOTIATOR_INTERVAL"] = "1"
        os.environ["_condor_SCHEDD_INTERVAL"] = "1"
        os.environ["_condor_SCHEDD_MIN_INTERVAL"] = "1"
        # Various required attributes for the startd
        os.environ["_condor_START"] = "TRUE"
        os.environ["_condor_SUSPEND"] = "FALSE"
        os.environ["_condor_CONTINUE"] = "TRUE"
        os.environ["_condor_PREEMPT"] = "FALSE"
        os.environ["_condor_KILL"] = "FALSE"
        os.environ["_condor_WANT_SUSPEND"] = "FALSE"
        os.environ["_condor_WANT_VACATE"] = "FALSE"
        os.environ["_condor_MachineMaxVacateTime"] = "5"
        os.environ["_condor_JOB_INHERITS_STARTER_ENVIRONMENT"] = "TRUE"
        htcondor.reload_config()
        htcondor.SecMan().invalidateAllSessions()

    def launch_daemons(self, daemons=["MASTER", "COLLECTOR"], config={}):
        makedirs_ignore_exist(htcondor.param["LOG"])
        makedirs_ignore_exist(htcondor.param["LOCK"])
        makedirs_ignore_exist(htcondor.param["EXECUTE"])
        makedirs_ignore_exist(htcondor.param["SPOOL"])
        makedirs_ignore_exist(htcondor.param["RUN"])
        remove_ignore_missing(htcondor.param["MASTER_ADDRESS_FILE"])
        remove_ignore_missing(htcondor.param["COLLECTOR_ADDRESS_FILE"])
        remove_ignore_missing(htcondor.param["SCHEDD_ADDRESS_FILE"])
        for key, val in config.items():
            os.environ["_condor_%s" % key] = val
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
                except:
                    e = sys.exc_info()[1]
                    print(str(e))
            finally:
                os._exit(1)
        global master_pid
        master_pid = self.pid
        for daemon in daemons:
            self.waitLocalDaemon(daemon)

    def tearDown(self):
        if self.pid > 1:
            global master_pid
            master_pid = 0
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


class TestPythonBindings(WithDaemons):

    def testDaemon(self):
        self.launch_daemons(["COLLECTOR"])

    def testLocate(self):
        self.launch_daemons(["COLLECTOR"])
        coll = htcondor.Collector()
        coll_ad = coll.locate(htcondor.DaemonTypes.Collector)
        self.assertTrue("MyAddress" in coll_ad)
        self.assertEquals(coll_ad["Name"].split(":")[-1], os.environ["_condor_PORT"])

    def testLocateList(self):
        self.launch_daemons(["COLLECTOR"])
        coll = htcondor.Collector()
        coll_ad = coll.locate(htcondor.DaemonTypes.Collector)
        self.assertTrue("MyAddress" in coll_ad)
        self.assertEquals(coll_ad["Name"].split(":")[-1], os.environ["_condor_PORT"])
        # Make sure we can pass a list of addresses
        coll = htcondor.Collector(["collector.example.com", coll_ad['Name']])
        coll_ad = coll.locate(htcondor.DaemonTypes.Collector)

    def testRemoteLocate(self):
        self.launch_daemons(["COLLECTOR"])
        coll = htcondor.Collector()
        coll_ad = coll.locate(htcondor.DaemonTypes.Collector)
        remote_ad = self.waitRemoteDaemon(htcondor.DaemonTypes.Collector, "%s@%s" % (htcondor.param["COLLECTOR_NAME"], htcondor.param["CONDOR_HOST"]))
        remote_address = remote_ad["MyAddress"].split(">")[0].split("?")[0].lower()
        coll_address = coll_ad["MyAddress"].split(">")[0].split("?")[0].lower()
        self.assertEquals(remote_address, coll_address)

    def testScheddLocate(self):
        self.launch_daemons(["SCHEDD", "COLLECTOR"])
        coll = htcondor.Collector()
        name = "%s@%s" % (htcondor.param["SCHEDD_NAME"], htcondor.param["CONDOR_HOST"])
        schedd_ad = self.waitRemoteDaemon(htcondor.DaemonTypes.Schedd, name, timeout=10)
        self.assertEquals(schedd_ad.eval("Name").lower(), name.lower())

    def testCollectorAdvertise(self):
        self.launch_daemons(["COLLECTOR"])
        coll = htcondor.Collector()
        now = time.time()
        ad = classad.ClassAd('[MyType="GenericAd"; Name="Foo"; Foo=1; Bar=%f; Baz="foo"]' % now)
        coll.advertise([ad])
        for i in range(5):
            ads = coll.query(htcondor.AdTypes.Any, 'Name =?= "Foo"', ["Bar"])
            if ads: break
            time.sleep(1)
        self.assertEquals(len(ads), 1)
        self.assertTrue(isinstance(ads[0]["Bar"], types.FloatType))
        self.assertEquals(ads[0]["Bar"], now)
        self.assertTrue("Foo" not in ads[0])

    def testScheddSubmit(self):
        self.launch_daemons(["SCHEDD", "COLLECTOR", "STARTD", "NEGOTIATOR"])
        output_file = os.path.join(testdir, "test.out")
        if os.path.exists(output_file):
            os.unlink(output_file)
        schedd = htcondor.Schedd()
        ad = classad.parse(open("tests/submit.ad"))
        ads = []
        cluster = schedd.submit(ad, 1, False, ads)
        #print ads[0]
        for i in range(60):
            ads = schedd.query("ClusterId == %d" % cluster, ["JobStatus"])
            #print ads
            if len(ads) == 0:
                break
            if i % 2 == 0:
                schedd.reschedule()
            time.sleep(1)
        self.assertEquals(open(output_file).read(), "hello world\n");

    def testScheddSubmitMany(self):
        self.launch_daemons(["SCHEDD", "COLLECTOR", "STARTD", "NEGOTIATOR"])
        output_file = os.path.join(testdir, "test.out")
        if os.path.exists(output_file):
            os.unlink(output_file)
        schedd = htcondor.Schedd()
        ad = classad.parse(open("tests/submit.ad"))
        ads = []
        cluster = schedd.submit(ad, 10, False, ads)
        #print ads[0]
        for i in range(60):
            ads = schedd.xquery("ClusterId == %d" % cluster, ["JobStatus"])
            ads = list(ads)
            #print ads
            if len(ads) == 0:
                break
            if i % 2 == 0:
                schedd.reschedule()
            time.sleep(1)
        self.assertEquals(open(output_file).read(), "hello world\n");

    def testScheddNonblockingQuery(self):
        self.launch_daemons(["SCHEDD", "COLLECTOR", "STARTD", "NEGOTIATOR"])
        output_file = os.path.join(testdir, "test.out")
        if os.path.exists(output_file):
            os.unlink(output_file)
        schedd = htcondor.Schedd()
        ad = classad.parse(open("tests/submit.ad"))
        ads = []
        cluster = schedd.submit(ad, 10, False, ads)
        for i in range(60):
            ads = schedd.xquery("ClusterId == %d" % cluster, ["JobStatus"])
            ads2 = schedd.xquery("ClusterId == %d" % cluster, ["JobStatus"])
            ctrs = [0, 0]
            iters = [(ads, 0), (ads2, 1)]
            while iters:
                for it, pos in iters:
                    try:
                        it.next()
                        ctrs[pos] += 1
                    except StopIteration:
                        iters.remove((it, pos))
            print ctrs
            if ctrs[0] == 0:
                break
            if i % 2 == 0:
                schedd.reschedule()
            time.sleep(1)
        self.assertEquals(open(output_file).read(), "hello world\n");

    def testScheddNonblockingQueryRemove(self):
        os.environ["_condor_SCHEDD_DEBUG"] = "D_FULLDEBUG|D_NETWORK"
        self.launch_daemons(["SCHEDD"])
        schedd = htcondor.Schedd()
        submit_ad = classad.parse(open("tests/submit_large.ad"))
        ads = []
        cluster = schedd.submit(submit_ad, 300, False, ads)
        ads = schedd.xquery("ClusterId == %d" % cluster)
        print str(datetime.datetime.now())
        print str(datetime.datetime.now())
        schedd.act(htcondor.JobAction.Remove, "ClusterId == %d" % cluster)
        time.sleep(3)
        print str(datetime.datetime.now())
        print len(list(ads))
        print str(datetime.datetime.now())

    def testScheddNonblockingQueryCount(self):
        os.environ["_condor_SCHEDD_DEBUG"] = "D_FULLDEBUG|D_NETWORK"
        self.launch_daemons(["SCHEDD"])
        schedd = htcondor.Schedd()
        submit_ad = classad.parse(open("tests/submit_large.ad"))
        schedd.act(htcondor.JobAction.Remove, "true")
        ads = []
        time.sleep(1)
        while ads:
            time.sleep(.2)
            ads = schedd.query("true")
        #print ads
        for i in range(1, 60):
            print "Testing querying %d jobs in queue." % i
            schedd.submit(submit_ad, i, True, ads)
            ads = schedd.query("true", ["ClusterID", "ProcID"])
            ads2 = list(schedd.xquery("true", ["ClusterID", "ProcID", "a1", "a2", "a3", "a4"]))
            #print ads
            #print ads2
            self.assertNotEqual(ads2[0].lookup("ProcID"), classad.Value.Undefined)
            for ad in ads:
                found_ad = False
                for ad2 in ads2:
                    if ad2["ProcID"] == ad["ProcID"] and ad2["ClusterID"] == ad["ClusterID"]:
                        found_ad = True
                        break
                self.assertTrue(found_ad, msg="Ad %s missing from xquery results: %s" % (ad, ads2))
            self.assertEquals(len(ads), i, msg="Old query protocol gives incorrect number of results (expected %d, got %d)" % (i, len(ads)))
            self.assertEquals(len(ads2), i, msg="New query protocol gives incorrect number of results (expected %d, got %d)" % (i, len(ads2)))
            schedd.act(htcondor.JobAction.Remove, "true")
            while ads:
                time.sleep(.2)
                ads = schedd.query("true")

    def testScheddSubmitSpool(self):
        self.launch_daemons(["SCHEDD", "COLLECTOR", "STARTD", "NEGOTIATOR"])
        output_file = os.path.join(testdir, "test.out")
        if os.path.exists(output_file):
            os.unlink(output_file)
        schedd = htcondor.Schedd()
        ad = classad.parse(open("tests/submit.ad"))
        result_ads = []
        cluster = schedd.submit(submit_ad, 1, True, result_ads)
        #print result_ads[0]
        schedd.spool(result_ads)
        for i in range(60):
            ads = schedd.query("ClusterId == %d" % cluster, ["JobStatus"])
            #print ads
            self.assertEquals(len(ads), 1)
            if ads[0]["JobStatus"] == 4:
                break
            if i % 5 == 0:
                schedd.reschedule()
            time.sleep(1)
        schedd.retrieve("ClusterId == %d" % cluster)
        #print "Final status:", schedd.query("ClusterId == %d" % cluster)[0];
        schedd.act(htcondor.JobAction.Remove, ["%d.0" % cluster])
        ads = schedd.query("ClusterId == %d" % cluster, ["JobStatus"])
        self.assertEquals(len(ads), 0)
        self.assertEquals(open(output_file).read(), "hello world\n");

    def testPing(self):
        self.launch_daemons(["COLLECTOR"])
        coll = htcondor.Collector()
        coll_ad = coll.locate(htcondor.DaemonTypes.Collector)
        self.assertTrue("MyAddress" in coll_ad)
        secman = htcondor.SecMan()
        authz_ad = secman.ping(coll_ad, "WRITE")
        self.assertTrue("AuthCommand" in authz_ad)
        self.assertEquals(authz_ad['AuthCommand'], 60021)
        self.assertTrue("AuthorizationSucceeded" in authz_ad)
        self.assertTrue(authz_ad['AuthorizationSucceeded'])

        authz_ad = secman.ping(coll_ad["MyAddress"], "WRITE")
        self.assertTrue("AuthCommand" in authz_ad)
        self.assertEquals(authz_ad['AuthCommand'], 60021)
        self.assertTrue("AuthorizationSucceeded" in authz_ad)
        self.assertTrue(authz_ad['AuthorizationSucceeded'])

        authz_ad = secman.ping(coll_ad["MyAddress"])
        self.assertTrue("AuthCommand" in authz_ad)
        self.assertEquals(authz_ad['AuthCommand'], 60011)
        self.assertTrue("AuthorizationSucceeded" in authz_ad)
        self.assertTrue(authz_ad['AuthorizationSucceeded'])

    def testEventLog(self):
        events = list(htcondor.read_events(open("tests/test_log.txt")))
        self.assertEquals(len(events), 4)
        a = dict(events[0])
        if 'CurrentTime' in a:
            del a['CurrentTime']
        b = {"LogNotes": "DAG Node: Job1",
             "MyType": "SubmitEvent",
             "EventTypeNumber": 0,
             "Subproc": 0,
             "Cluster": 236467,
             "Proc": 0,
             "EventTime": "%d-11-15T17:05:55" % datetime.datetime.now().year,
             "SubmitHost": "<169.228.38.38:9615?sock=18627_6227_3>",
            }
        self.assertEquals(set(a.keys()), set(b.keys()))
        for key, val in a.items():
            self.assertEquals(val, b[key])

    def testTransaction(self):
        self.launch_daemons(["SCHEDD", "COLLECTOR", "STARTD", "NEGOTIATOR"])
        output_file = os.path.join(testdir, "test.out")
        log_file = os.path.join(testdir, "test.log")
        if os.path.exists(output_file):
            os.unlink(output_file)
        if os.path.exists(log_file):
            os.unlink(log_file)
        schedd = htcondor.Schedd()
        ad = classad.parse(open("tests/submit_sleep.ad"))
        result_ads = []
        cluster = schedd.submit(ad, 1, True, result_ads)

        with schedd.transaction() as txn:
            schedd.edit(["%d.0" % cluster], 'foo', classad.Literal(1))
            schedd.edit(["%d.0" % cluster], 'bar', classad.Literal(2))
        ads = schedd.query("ClusterId == %d" % cluster, ["JobStatus", 'foo', 'bar'])
        self.assertEquals(len(ads), 1)
        self.assertEquals(ads[0]['foo'], 1)
        self.assertEquals(ads[0]['bar'], 2)

        with schedd.transaction() as txn:
            schedd.edit(["%d.0" % cluster], 'baz', classad.Literal(3))
            with schedd.transaction(htcondor.TransactionFlags.NonDurable | htcondor.TransactionFlags.ShouldLog, True) as txn:
                schedd.edit(["%d.0" % cluster], 'foo', classad.Literal(4))
                schedd.edit(["%d.0" % cluster], 'bar', classad.Literal(5))
        ads = schedd.query("ClusterId == %d" % cluster, ["JobStatus", 'foo', 'bar', 'baz'])
        self.assertEquals(len(ads), 1)
        self.assertEquals(ads[0]['foo'], 4)
        self.assertEquals(ads[0]['bar'], 5)
        self.assertEquals(ads[0]['baz'], 3)

        try:
            with schedd.transaction() as txn:
                schedd.edit(["%d.0" % cluster], 'foo', classad.Literal(6))
                schedd.edit(["%d.0" % cluster], 'bar', classad.Literal(7))
                raise Exception("force abort")
        except:
            exctype, e = sys.exc_info()[:2]
            if not issubclass(exctype, Exception):
                raise
            self.assertEquals(str(e), "force abort")
        ads = schedd.query("ClusterId == %d" % cluster, ["JobStatus", 'foo', 'bar'])
        self.assertEquals(len(ads), 1)
        self.assertEquals(ads[0]['foo'], 4)
        self.assertEquals(ads[0]['bar'], 5)

        try:
            with schedd.transaction() as txn:
                schedd.edit(["%d.0" % cluster], 'baz', classad.Literal(8))
                with schedd.transaction(htcondor.TransactionFlags.NonDurable | htcondor.TransactionFlags.ShouldLog, True) as txn:
                    schedd.edit(["%d.0" % cluster], 'foo', classad.Literal(9))
                    schedd.edit(["%d.0" % cluster], 'bar', classad.Literal(10))
                raise Exception("force abort")
        except:
            exctype, e = sys.exc_info()[:2]
            if not issubclass(exctype, Exception): 
                raise
            self.assertEquals(str(e), "force abort")
        ads = schedd.query("ClusterId == %d" % cluster, ["JobStatus", 'foo', 'bar', 'baz'])
        self.assertEquals(len(ads), 1)
        self.assertEquals(ads[0]['foo'], 4)
        self.assertEquals(ads[0]['bar'], 5)
        self.assertEquals(ads[0]['baz'], 3)

        schedd.act(htcondor.JobAction.Remove, ["%d.0" % cluster])
        ads = schedd.query("ClusterId == %d" % cluster, ["JobStatus"])
        self.assertEquals(len(ads), 0)


if __name__ == '__main__':
    unittest.main()

