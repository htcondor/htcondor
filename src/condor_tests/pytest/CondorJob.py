import classad
import htcondor
import os
import time

from PersonalCondor import PersonalCondor
from Globals import *
from Utils import Utils

class CondorJob(object):

    def __init__(self, job_args):
        self._cluster_id = None
        self._job_args = job_args

    def FailureCallback(self):
        Utils.TLog("Job cluster " + str(self._cluster_id) + " failure callback invoked")

    def SubmitCallback(self):
        Utils.TLog("Job cluster " + str(self._cluster_id) + " submit callback invoked")

    def SuccessCallback(self):
        Utils.TLog("Job cluster " + str(self._cluster_id) + " success callback invoked")


    def Submit(self, wait=False, count=1):

        # Submit the job defined by submit_args
        Utils.TLog("Submitting job with arguments: " + str(self._job_args))
        schedd = htcondor.Schedd()
        submit = htcondor.Submit(self._job_args)
        try:
            with schedd.transaction() as txn:
                self._cluster_id = submit.queue(txn, count)
        except Exception as e:
            print( "Job submission failed for an unknown error: " + str(e) )
            return JOB_FAILURE

        Utils.TLog("Job running on cluster " + str(self._cluster_id))

        # Wait until job has finished running?
        if wait is True:
            submit_result = self.WaitForFinish()
            return submit_result

        # If we aren't waiting for finish, return None
        return None


    def WaitForFinish(self, timeout=240):

        schedd = htcondor.Schedd()
        for i in range(timeout):
            ads = schedd.query("ClusterId == %d" % self._cluster_id, ["JobStatus"])
            Utils.TLog("Ads = " + str(ads))

            # When the job is complete, ads will be empty
            if len(ads) == 0:
                self.SuccessCallback()
                return JOB_SUCCESS
            else:
                status = ads[0]["JobStatus"]
                if status == 5:
                    Utils.TLog("Job was placed on hold. Aborting.")
                    self.FailureCallback()
                    return JOB_FAILURE
            time.sleep(1)

        # If we got this far, we hit the timeout and job did not complete
        Utils.TLog("Job failed to complete with timeout = " + str(timeout))
        return JOB_FAILURE


    def RegisterSubmit(self, submit_callback_fn):
        self.SubmitCallback = submit_callback_fn


    def RegisterSuccess(self, success_callback_fn):
        self.SuccessCallback = success_callback_fn


    def RegisterFailure(self, failure_callback_fn):
        self.FailureCallback = failure_callback_fn
