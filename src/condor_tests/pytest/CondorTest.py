import datetime
import errno
import htcondor
import os
import subprocess
import sys
import time

from PersonalCondor import PersonalCondor
from Globals import *
from Utils import Utils


class CondorTest(object):

    def __init__(self, name, params=None):
        self._name = name
        self._params = params
        self._personal_condors = dict()


    def __del__(self):
        self.End()


    # @return: -1 if error, or the numeric key >=0 of this PersonalCondor instance if success
    def StartPersonalCondor(self):
        personal = PersonalCondor(self._name, self._params)
        success = personal.Start()
        if success is not True:
            return -1
        self._personal_condors[0] = personal
        return 0


    def End(self):
        try:
            for key, personal in self._personal_condors.items():
                personal.Stop()
                del self._personal_condors[key]
        except:
            Utils.TLog("PersonalCondor was not running, ending test")
