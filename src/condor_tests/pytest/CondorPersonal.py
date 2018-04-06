import classad
import errno
import htcondor
import os
import subprocess
import sys
import time

from CondorUtils import CondorUtils

class CondorPersonal(object):

    def __init__(self, name):
        self._name = name
        self._env_dir = name + ".env"
        self._env_path = os.getcwd() + "/" + self._env_dir
        CondorUtils.Debug("CondorPersonal initialized under path: " + self._env_path)

    def StartCondor(self):

        self.SetupEnvironment()

        try:
            p = subprocess.Popen(["condor_master", "-f &"])
            if p < 0:
                CondorUtils.Debug("Child was terminated by signal: " + str(p))
                return False
        except OSError as e:
            CondorUtils.Debug("Execution of condor_master failed: " + e)
            return False

        self._master_pid = p.pid
        CondorUtils.Debug("Started a new personal condor with condor_master pid " + str(self._master_pid))
        
        return True


    def ShutdownCondor(self):
        os.system("kill " + str(self._master_pid))


    # Sets up local configuration options we'll use to stand up the personal Condor instance.
    def SetupEnvironment(self):

        os.system("mkdir -p " + self._env_dir)
        os.system("mkdir -p " + self._env_dir + "/execute")
        os.system("mkdir -p " + self._env_dir + "/log")
        os.system("mkdir -p " + self._env_dir + "/lock")
        os.system("mkdir -p " + self._env_dir + "/run")
        os.system("mkdir -p " + self._env_dir + "/spool")
        
        self.RemoveIgnoreMissing(htcondor.param["MASTER_ADDRESS_FILE"])
        self.RemoveIgnoreMissing(htcondor.param["COLLECTOR_ADDRESS_FILE"])
        self.RemoveIgnoreMissing(htcondor.param["SCHEDD_ADDRESS_FILE"])

        # MRC: Why does this work without setting the collector to a different port?
        #os.environ["_condor_COLLECTOR_PORT"] = "9718"
        os.environ["_condor_LOCAL_DIR"] = self._env_path
        os.environ["_condor_EXECUTE"] =  '$(LOCAL_DIR)/execute'
        os.environ["_condor_LOG"] =  '$(LOCAL_DIR)/log'
        os.environ["_condor_LOCK"] = '$(LOCAL_DIR)/lock'
        os.environ["_condor_RUN"] = '$(LOCAL_DIR)/run'
        os.environ["_condor_SPOOL"] = '$(LOCAL_DIR)/spool'

        # MRC: We don't need to make a condor_config file AND set OS environment variables.
        # Should probably remove this, unless it's more useful than OS variables?
        config = "COLLECTOR_PORT = 9718\n"
        config_file = open(self._env_dir + "/condor_config", "w")
        config_file.write(config)
        config_file.close()

        # MRC: What does this function actually do?
        htcondor.reload_config()

    def RemoveIgnoreMissing(self, file):
        try:
            os.unlink(file)
        except:
            exctype, oe = sys.exc_info()[:2]
            if not issubclass(exctype, OSError): raise
            if oe.errno != errno.ENOENT:
                raise
