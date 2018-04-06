import classad
import htcondor
import os
import time

from CondorPersonal import CondorPersonal
from CondorUtils import CondorUtils


class CondorPytest(object):

    def __init__(self, name):
        self._name = name


    def RunTest(self, submit_args):

        success = False

        # Setup a personal condor environment
        CondorUtils.Debug("Attempting to start test " + self._name)
        personal = CondorPersonal(self._name)
        start_success = personal.StartCondor()
        if start_success is False:
            CondorUtils.Debug("Failed to start the personal condor environment. Exiting.")
            return False
        else:
            CondorUtils.Debug("Personal condor environment started successfully!")

        # Wait until we're sure all daemons have started
        # MRC: Can we do this using python bindings? Or do we need to use condor_who?
        time.sleep(5)

        # Submit the test defined by submit_args
        CondorUtils.Debug("Submitting the test job...")
        schedd = htcondor.Schedd()
        submit = htcondor.Submit(submit_args)
        try:
            with schedd.transaction() as txn:
                cluster = submit.queue(txn)
        except:
            print("Job submission failed for an unknown error")
            personal.ShutdownCondor()
            return False
                
        CondorUtils.Debug("Test job running on cluster " + str(cluster))

        # Wait until test has finished running by watching the job status
        # MRC: Timeout should be user configurable
        timeout = 240
        for i in range(timeout):
            ads = schedd.query("ClusterId == %d" % cluster, ["JobStatus"])
            CondorUtils.Debug("Ads = " + str(ads))
            # When the job is complete, ads will be empty
            if len(ads) == 0:
                success = True
                break
            else:
                status = ads[0]["JobStatus"]
                if status == 5:
                    CondorUtils.Debug("Job was placed on hold. Aborting.")
                    break
            time.sleep(1)

        # Shutdown personal condor environment + cleanup
        personal.ShutdownCondor()

        return success



