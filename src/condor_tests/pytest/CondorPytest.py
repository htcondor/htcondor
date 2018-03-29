import htcondor
import os
import time

from CondorPersonal import CondorPersonal
from CondorUtils import CondorUtils


class CondorPytest(object):

    def __init__(self, name):
        self._name = name


    def StartTest(self):

        # Setup a personal condor environment
        personal = CondorPersonal(self._name)
        start_success = personal.StartCondor()
        if start_success is False:
            CondorUtils.Debug("Failed to start the personal condor environment. Exiting.")
            sys.exit(1)
        else:
            CondorUtils.Debug("Personal condor environment started successfully!")

        # Wait until we're sure all daemons have started
        time.sleep(15)

        # Submit the test

        # Wait until test has finished running, with a timeout


        # Shutdown personal condor environment + cleanup
        personal.ShutdownCondor()




