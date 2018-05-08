import classad
import htcondor
import os
import time

from PersonalCondor import PersonalCondor
from CondorTest import CondorTest

JOB_SUCCESS = 0
JOB_FAILURE = 1


class CondorJob(object):


    def __init__(self, job_args):
        self._job_args = job_args


    def Run(self):

        # Submit the job defined by submit_args
        CondorTest.TLog("Running job with arguments: " + str(self._job_args))
        schedd = htcondor.Schedd()
        submit = htcondor.Submit(self._job_args)
        try:
            with schedd.transaction() as txn:
                cluster = submit.queue(txn)
        except:
            print("Job submission failed for an unknown error")
            personal.ShutdownCondor()
            return JOB_FAILURE

        CondorTest.TLog("Job running on cluster " + str(cluster))

        # Wait until test has finished running by watching the job status
        # MRC: Timeout should be user configurable
        job_timeout = 240
        for i in range(job_timeout):
            ads = schedd.query("ClusterId == %d" % cluster, ["JobStatus"])
            CondorTest.TLog("Ads = " + str(ads))
            # When the job is complete, ads will be empty
            if len(ads) == 0:
                break
            else:
                status = ads[0]["JobStatus"]
                if status == 5:
                    CondorTest.TLog("Job was placed on hold. Aborting.")
                    return JOB_FAILURE
            time.sleep(1)

        # If we got this far, we assume the job succeeded.
        return JOB_SUCCESS
