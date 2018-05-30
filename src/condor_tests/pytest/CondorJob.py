import classad
import htcondor
import os
import time

from PersonalCondor import PersonalCondor
from Utils import Utils

JOB_SUCCESS = 0
JOB_FAILURE = 1


class CondorJob(object):

    def __init__(self, job_args):
        self._job_args = job_args


    def Submit(self, wait=True):

        # Submit the job defined by submit_args
        Utils.TLog("Submitting job with arguments: " + str(self._job_args))
        schedd = htcondor.Schedd()
        submit = htcondor.Submit(self._job_args)
        try:
            with schedd.transaction() as txn:
                self._cluster_id = submit.queue(txn)
        except:
            print("Job submission failed for an unknown error")
            return JOB_FAILURE

        Utils.TLog("Job running on cluster " + str(self._cluster_id))

        # Wait until job has finished running?
        if wait is True:
            self.WaitForFinish()

        # If we got this far, we assume the job succeeded.
        return JOB_SUCCESS


    def WaitForFinish(self, timeout=240):

        schedd = htcondor.Schedd()
        for i in range(timeout):
            ads = schedd.query("ClusterId == %d" % self._cluster_id, ["JobStatus"])
            Utils.TLog("Ads = " + str(ads))
            # When the job is complete, ads will be empty
            if len(ads) == 0:
                break
            else:
                status = ads[0]["JobStatus"]
                if status == 5:
                    Utils.TLog("Job was placed on hold. Aborting.")
                    return JOB_FAILURE
            time.sleep(1)