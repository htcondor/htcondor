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
        self._old_status = None

	def clusterID(self):
		return self._cluster_id

    ##
    ## Callbacks used by WaitForJob().
    ##

    #
    # Job-state callbacks.  Called when the job's state changes to the
    # corresponding state (including the initial state).
    #

    def IdleCallback(self):
        return None

    def RunningCallback(self):
        return None

    def RemovedCallback(self):
        return None

    def CompletedCallback(self):
        return None

    def HeldCallback(self):
        Utils.TLog("Job was place on hold. Aborting.")
        self.FailureCallback()
        return JOB_FAILURE

    def TransferringOutputCallback(self):
        return None

    def SuspendedCallback(self):
        return None;

    #
    # Semantic callbacks.  Note note WaitForFinish() will return JOB_FAILURE
    # but _not_ call the failure callback if it times out.
    #
    #   Success: when the job exits the queue.
    #   Failure: when the job goes on hold.
    #   Submit: presently not called (FIXME).
    #

    def SuccessCallback(self):
        Utils.TLog("Job cluster " + str(self._cluster_id) + " success callback invoked")

    def FailureCallback(self):
        Utils.TLog("Job cluster " + str(self._cluster_id) + " failure callback invoked")

    def SubmitCallback(self):
        Utils.TLog("Job cluster " + str(self._cluster_id) + " submit callback invoked")


    def Submit(self, wait=False):

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
            submit_result = self.WaitForFinish()
            return submit_result

        # If we aren't waiting for finish, return None
        return None


    # FIXME: Do we actually want to poll the schedd, or should we be reading the log?
    def WaitForFinish(self, timeout=240):

        schedd = htcondor.Schedd()
        for i in range(timeout):
            ads = schedd.query("ClusterId == %d" % self._cluster_id, ["JobStatus"])
            Utils.TLog("Ads = " + str(ads))

            # When the job is complete, ads will be empty
            if len(ads) == 0:
                self.SuccessCallback()
                return JOB_SUCCESS

			# FIXME: If it's ever important to have multiple jobs in the same
			# cluster for testing, we'll need to fix this -- look at each
			# proc's status (and record it) individually, and then call the
			# callbacks with the proc ID.  (Note that we won't have to record
			# the statuses if switch to reading the user log.)  On the other
			# hand, if we ever want to wait on the status of more than one
			# job at the same time, we'll need a different API.
            status = ads[0]["JobStatus"]
            if not (self._old_status == None or self._old_status != status):
                continue

            callbackName = JobStatus.Number[ status ] + "Callback"
            method = getattr( self, callbackName )
            callbackResult = method()
            if( callbackResult != None ):
                return callbackResult

            self._old_status = status
            time.sleep(1)

        # If we got this far, we hit the timeout and job did not complete
        Utils.TLog("Job failed to complete with timeout = " + str(timeout))
        return JOB_FAILURE

    def RegisterIdle(self, idle_callback_fn):
        self.IdleCallback = idle_callback_fn

    def RegisterRunning(self, running_callback_fn):
        self.RunningCallback = running_callback_fn

    def RegisterRemoved(self, removed_callback_fn):
        self.RemovedCallback = removed_callback_fn

    def RegisterCompleted(self, completed_callback_fn):
        self.CompletedCallback = completed_callback_fn

    def RegisterHeld(self, held_callback_fn):
        self.HeldCallback = held_callback_fn

    def RegisterTransferringOutput(self, to_callback_fn):
        self.TransferringOutputCallback = to_callback_fn

    def RegisterSuspended(self, suspended_callback_fn):
        self.SuspendedCallback = suspended_callback_fn


    def RegisterSuccess(self, success_callback_fn):
        self.SuccessCallback = success_callback_fn

    def RegisterFailure(self, failure_callback_fn):
        self.FailureCallback = failure_callback_fn

    def RegisterSubmit(self, submit_callback_fn):
        self.SubmitCallback = submit_callback_fn

