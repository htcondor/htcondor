import classad
import htcondor
import os
import time

from CondorPersonal import CondorPersonal
from CondorUtils import CondorUtils

JOB_SUCCESS = 0
JOB_FAILURE = 1

class CondorPytest(object):

    def __init__(self, name):
        self._name = name


    # RunJob
    def RunJob(self, submit_args):

        # Setup a personal condor environment
        CondorUtils.TLog("Attempting to start test " + self._name)
        personal = CondorPersonal(self._name)
        start_success = personal.StartCondor()
        if start_success is False:
            CondorUtils.TLog("Failed to start the personal condor environment. Exiting.")
            return JOB_FAILURE
        else:
            CondorUtils.TLog("Personal condor environment started successfully!")

        # Wait until we're sure all daemons have started
        is_ready = self.WaitForReadyDaemons(personal)
        if is_ready is False:
            CondorUtils.TLog("Condor daemons did not enter reaedy state. Exiting.")
            return JOB_FAILURE

        # Submit the test defined by submit_args
        CondorUtils.TLog("Submitting the test job...")
        schedd = htcondor.Schedd()
        submit = htcondor.Submit(submit_args)
        try:
            with schedd.transaction() as txn:
                cluster = submit.queue(txn)
        except:
            print("Job submission failed for an unknown error")
            personal.ShutdownCondor()
            return JOB_FAILURE
                
        CondorUtils.TLog("Test job running on cluster " + str(cluster))

        # Wait until test has finished running by watching the job status
        # MRC: Timeout should be user configurable
        test_timeout = 240
        for i in range(test_timeout):
            ads = schedd.query("ClusterId == %d" % cluster, ["JobStatus"])
            CondorUtils.TLog("Ads = " + str(ads))
            # When the job is complete, ads will be empty
            if len(ads) == 0:
                break
            else:
                status = ads[0]["JobStatus"]
                if status == 5:
                    CondorUtils.TLog("Job was placed on hold. Aborting.")
                    return JOB_FAILURE
            time.sleep(1)

        # Shutdown personal condor environment + cleanup
        personal.ShutdownCondor()

        # If we got this far, we assume the job succeeded.
        return JOB_SUCCESS

    # MRC: Eventually want to do this using python bindings
    def WaitForReadyDaemons(self, personal):
        is_ready_attempts = 6
        for i in range(is_ready_attempts):
            time.sleep(5)
            CondorUtils.TLog("Checking for condor_who output")
            who_output = CondorUtils.RunCondorTool("condor_who -quick -daemon -log " + personal._log_path)
            # Parse the output from condor_who. We're looking for "IsReady = true"
            #CondorUtils.TLog(who_output)
            for line in iter(who_output.splitlines()):
                print(line)
                if line == "IsReady = true":
                    return True

        # If we got this far, we never saw the IsReady notice. Return false
        return False

