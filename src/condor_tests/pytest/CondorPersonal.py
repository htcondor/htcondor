import htcondor
import os
import subprocess
import sys
import time

from CondorUtils import CondorUtils

class CondorPersonal(object):

    def __init__(self, name):
        self._name = name
        self._env_dir = name + ".env/"
        CondorUtils.Debug("CondorPersonal initialized under directory: " + self._env_dir)

    def StartCondor(self):

        self.SetupPersonalEnvironment()

        try:
            p = subprocess.Popen(["condor_master", "-f &"])
            if p < 0:
                CondorUtils.Debug("Child was terminated by signal: " + str(p))
                return False
            else:
                CondorUtils.Debug("Child returned: " + str(p))
        except OSError as e:
            CondorUtils.Debug("Execution of condor_master failed: " + e)
            return False

        self._master_pid = p.pid
        CondorUtils.Debug("Started a new personal condor with condor_master pid " + str(self._master_pid))

        return True


    def ShutdownCondor(self):
        os.system("kill " + str(self._master_pid))


    # Sets up local configuration options we'll use to stand up the personal Condor instance.
    def SetupPersonalEnvironment(self):

        os.system("mkdir -p " + self._env_dir)

        os.environ["_condor_COLLECTOR_PORT"] = "9718"
        os.environ["_condor_LOCK"] = self._env_dir

        config = "COLLECTOR_PORT = 9718"

        config_file = open(self._env_dir + "condor_config", "w")
        config_file.write(config)
        config_file.close()

        CondorUtils.Debug("Checking COLLECTOR_PORT:")
        os.system("condor_config_val COLLECTOR_PORT")

        htcondor.reload_config()

