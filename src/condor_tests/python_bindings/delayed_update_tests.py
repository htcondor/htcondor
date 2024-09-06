
import os
import time
import unittest

import classad2 as classad
import htcondor2 as htcondor

import htcondor_tests

class TestChirp(htcondor_tests.WithDaemons):

    def runJob(self, ad):
        schedd = htcondor.Schedd()
        ads = []
        cluster = schedd.submit(ad, 1, False, ads)
        for i in range(60):
            ads = schedd.query("ClusterId == %d" % cluster, ["JobStatus"])
            if len(ads) == 0:
                break
            if i % 2 == 0:
                schedd.reschedule()
            time.sleep(1)
        return cluster

    def getLastHistory(self, cluster):
        fd = os.popen("condor_history -match 1 -l %d" % cluster)
        ad = classad.parseOld(fd.read()[:-1])
        self.assertFalse(fd.close())
        return ad

    def tryDelayedUpdate(self, prefix="Chirp", shouldwork=True, wantupdate=None):
        output_file = os.path.join(htcondor_tests.testdir, "test.out")
        if os.path.exists(output_file):
            os.unlink(output_file)
        ad = classad.parse(open("tests/delayed_submit.ad"))
        ad["Arguments"] = "--prefix=%s --type=delayed --shouldwork=%s" % (prefix, str(shouldwork))
        if wantupdate:
            ad["WantDelayedUpdates"] = True
        elif not wantupdate:
            ad["WantDelayedUpdates"] = False
        cluster = self.runJob(ad)
        result_ad = self.getLastHistory(cluster)
        attr = "%sFoo" % prefix
        self.assertTrue("ExitCode" in result_ad)
        self.assertEqual(result_ad["ExitCode"], 0)
        last_line = open(output_file).readlines()[-1]
        self.assertEqual(last_line, "SUCCESS\n")
        if shouldwork:
            self.assertTrue(attr in result_ad)
            self.assertEqual(result_ad[attr], 2)

    def tryIO(self, shouldwork=True, wantio=None):
        open(os.path.join(htcondor_tests.testdir, "test_chirp_io"), "w").write("hello world")
        output_file = os.path.join(htcondor_tests.testdir, "test.out")
        if os.path.exists(output_file):
            os.unlink(output_file)
        ad = classad.parse(open("tests/delayed_submit.ad"))
        ad["Arguments"] = "--type=io --shouldwork=%s" % str(shouldwork)
        if wantio:
            ad["WantIOProxy"] = True
        elif not wantio:
            ad["WantIOProxy"] = False
        cluster = self.runJob(ad)
        result_ad = self.getLastHistory(cluster)
        self.assertTrue("ExitCode" in result_ad)
        self.assertEqual(result_ad["ExitCode"], 0)
        last_line = open(output_file).readlines()[-1]
        self.assertEqual(last_line, "SUCCESS\n")

    def tryUpdate(self, shouldwork=True, wantio = None, wantupdate=None, prefix="NonChirp"):
        output_file = os.path.join(htcondor_tests.testdir, "test.out")
        if os.path.exists(output_file):
            os.unlink(output_file)
        ad = classad.parse(open("tests/delayed_submit.ad"))
        ad["Arguments"] = "--type=update --prefix=%s --shouldwork=%s" % (prefix, str(shouldwork))
        if wantio:
            ad["WantIOProxy"] = True
        elif not wantio:
            ad["WantIOProxy"] = False
        if wantupdate:
            ad["WantRemoteUpdates"] = True
        elif not wantupdate:
            ad["WantRemoteUpdates"] = False
        cluster = self.runJob(ad)
        result_ad = self.getLastHistory(cluster)
        self.assertTrue("ExitCode" in result_ad)
        self.assertEqual(result_ad["ExitCode"], 0)
        last_line = open(output_file).readlines()[-1]
        self.assertEqual(last_line, "SUCCESS\n")
        if shouldwork:
            attr = "%sFoo" % prefix
            self.assertTrue(attr in result_ad)
            self.assertEqual(result_ad[attr], 2)

    def testDelayedUpdate(self):
        self.launch_daemons(["SCHEDD", "COLLECTOR", "STARTD", "NEGOTIATOR"])
        self.tryIO(shouldwork = False)
        self.tryIO(shouldwork = True, wantio = True)
        self.tryUpdate(shouldwork = False)
        self.tryUpdate(shouldwork = True, wantio = True)
        self.tryUpdate(shouldwork = True, wantupdate = True)
        self.tryDelayedUpdate(shouldwork = True)
        self.tryDelayedUpdate(shouldwork = True, wantupdate = True)
        self.tryDelayedUpdate(shouldwork = False, wantupdate = False)
        self.tryDelayedUpdate(shouldwork = False, prefix="Nope")
        self.tryDelayedUpdate(shouldwork = True, prefix="chirp")

    def testDelayedUpdateSecurity(self):
        self.launch_daemons(["SCHEDD", "COLLECTOR", "STARTD", "NEGOTIATOR"], {"ENABLE_CHIRP_DELAYED": "FALSE"})
        self.tryDelayedUpdate(shouldwork = False)
        self.tryDelayedUpdate(shouldwork = False, wantupdate = True)

    def testDelayedUpdateSecurity2(self):
        self.launch_daemons(["SCHEDD", "COLLECTOR", "STARTD", "NEGOTIATOR"], {"ENABLE_CHIRP": "FALSE"})
        self.tryIO(shouldwork = False)
        self.tryIO(shouldwork = False, wantio = True)
        self.tryUpdate(shouldwork = False)
        self.tryUpdate(shouldwork = False, wantupdate = True)
        self.tryDelayedUpdate(shouldwork = False)

    def testDelayedUpdateSecurity3(self):
        self.launch_daemons(["SCHEDD", "COLLECTOR", "STARTD", "NEGOTIATOR"], {"ENABLE_CHIRP_IO": "FALSE"})
        self.tryIO(shouldwork = False, wantio = True)
        self.tryUpdate(shouldwork = False, wantio = True)
        self.tryUpdate(shouldwork = True, wantupdate = True)
        self.tryDelayedUpdate(shouldwork = True)

    def testDelayedUpdateSecurity4(self):
        self.launch_daemons(["SCHEDD", "COLLECTOR", "STARTD", "NEGOTIATOR"], {"ENABLE_CHIRP_UPDATES": "FALSE"})
        self.tryIO(shouldwork = True, wantio = True)
        self.tryUpdate(shouldwork = False, wantio = True)
        self.tryUpdate(shouldwork = False, wantupdate = True)
        self.tryDelayedUpdate(shouldwork = True)

    def testDelayedUpdatePrefix(self):
        self.launch_daemons(["SCHEDD", "COLLECTOR", "STARTD", "NEGOTIATOR"], {"CHIRP_DELAYED_UPDATE_PREFIX": "Foo*"})
        self.tryDelayedUpdate(shouldwork = False)
        self.tryDelayedUpdate(prefix="foo")

    def testDelayedUpdateDOS(self):
        self.launch_daemons(["SCHEDD", "COLLECTOR", "STARTD", "NEGOTIATOR"])
        output_file = os.path.join(htcondor_tests.testdir, "test.out")
        if os.path.exists(output_file):
            os.unlink(output_file)
        ad = classad.parse(open("tests/delayed_submit.ad"))
        ad["Arguments"] = "--type=delayeddos"
        cluster = self.runJob(ad)
        result_ad = self.getLastHistory(cluster)
        self.assertTrue("ExitCode" in result_ad)
        self.assertEqual(result_ad["ExitCode"], 0)
        last_line = open(output_file).readlines()[-1]
        self.assertEqual(last_line, "SUCCESS\n")
        self.assertTrue("ChirpFoo" in result_ad)
        self.assertEqual(result_ad["ChirpFoo"], "0" * 990)
        self.assertFalse("ChirpBar" in result_ad)
        for i in range(1, 50):
            self.assertTrue(("ChirpFoo%d" % i) in result_ad)
        self.assertFalse("ChirpFoo50" in result_ad)

if __name__ == '__main__':
    unittest.main()

