import datetime
import errno
import htcondor
import os
import subprocess
import sys
import time

from PersonalCondor import PersonalCondor
from Utils import Utils

JOB_SUCCESS = 0
JOB_FAILURE = 1

TEST_SUCCESS = 0
TEST_FAILURE = 1


class CondorTest(object):

    def __init__(self, name):
        self._name = name
        self._personal_condors = dict()


    def __del__(self):
        self.End()


    def _success_callback_fn(self):
        sys.exit(0)


    def _failure_callback_fn(self):
        sys.exit(1)


    # @return: -1 if error, or the numeric key >=0 of this PersonalCondor instance if success
    def StartPersonalCondor(self):
        personal = PersonalCondor(self._name)
        success = personal.Start()
        if success is not True:
            return -1
        self._personal_condors[0] = personal
        return 0


    def SubmitJob(self, job, wait=True, key=0):

        self._submit_callback_fn()
        
        # If no PersonalCondor objects are registered to this test, submit to system condor
        if len(self._personal_condors) == 0:
            job.Submit(wait)
        # If a single PersonalCondor object is registered, update the system CONDOR_CONFIG 
        # to point to that object's condor_config.local, then submit the test
        elif len(self._personal_condors) == 1:
            for key, personal in self._personal_condors.items():
                personal.SetCondorConfig()
                job.Submit(wait)
        # If multiple PersonalCondor objects are registered, check the user-supplied key
        # value, update CONDOR_CONFIG to point to the specific instance, then submit the test
        elif len(self._personal_condors) > 1:
            self._personal_condors[key].SetCondorConfig()
            result = job.Submit(wait)
            if result == JOB_SUCESS:
                self._success_callback_fn()
            elif result == JOB_FAILURE:
                self._failure_callback_fn()
            


    def RegisterSubmit(self, _submit_callback_fn):
        self._submit_callback_fn = _submit_callback_fn


    def RegisterSuccess(self, _success_callback_fn):
        self._success_callback_fn = _success_callback_fn


    def RegisterFailure(self, _failure_callback_fn):
        self._failure_callback_fn = _failure_callback_fn


    def End(self):
        try:
            for key, personal in self._personal_condors.items():
                personal.Stop()
                del self._personal_condors[key]
        except:
            Utils.TLog("PersonalCondor was not running, ending test")
