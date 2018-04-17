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
        self._local_dir = name + ".local"
        self._local_path = os.getcwd() + "/" + self._local_dir
        self._local_config = self._local_path + "/condor_config.local"
        CondorUtils.Debug("CondorPersonal initialized under path: " + self._local_path)

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
        os.system("condor_off -master")

    # Sets up local configuration options we'll use to stand up the personal Condor instance.
    def SetupEnvironment(self):

        self.MakedirsIgnoreExist(self._local_dir)
        self.MakedirsIgnoreExist(self._local_dir + "/execute")
        self.MakedirsIgnoreExist(self._local_dir + "/log")
        self.MakedirsIgnoreExist(self._local_dir + "/lock")
        self.MakedirsIgnoreExist(self._local_dir + "/run")
        self.MakedirsIgnoreExist(self._local_dir + "/spool")

        self.RemoveIgnoreMissing(htcondor.param["MASTER_ADDRESS_FILE"])
        self.RemoveIgnoreMissing(htcondor.param["COLLECTOR_ADDRESS_FILE"])
        self.RemoveIgnoreMissing(htcondor.param["SCHEDD_ADDRESS_FILE"])

        # Create a config file in this test's local directory based on existing
        # config settings
        os.system("condor_config_val -write:up " + self._local_config)

        # Add whatever internal testing config values we need
        config = "LOCAL_DIR = " + self._local_path + "\n"
        config += "EXECUTE = $(LOCAL_DIR)/execute\n"
        config += "LOCK = $(LOCAL_DIR)/lock\n"
        config += "RUN = $(LOCAL_DIR)/run\n"
        config += "SPOOL = $(LOCAL_DIR)/spool\n"
        config += "COLLECTOR_HOST = $(CONDOR_HOST):0\n"
        config_file = open(self._local_config, "a")
        config_file.write(config)
        config_file.close()
        os.environ["CONDOR_CONFIG"] = self._local_config

        # MRC: What does this function actually do?
        htcondor.reload_config()

    def MakedirsIgnoreExist(self, directory):
        try:
            CondorUtils.Debug("Making directory " + str(directory))
            os.makedirs(directory)
        except:
            exctype, oe = sys.exc_info()[:2]
            if not issubclass(exctype, OSError): raise
            if oe.errno != errno.EEXIST:
                raise

    def RemoveIgnoreMissing(self, file):
        try:
            os.unlink(file)
        except:
            exctype, oe = sys.exc_info()[:2]
            if not issubclass(exctype, OSError): raise
            if oe.errno != errno.ENOENT:
                raise
