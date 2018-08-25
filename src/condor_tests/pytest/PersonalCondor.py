import classad
import errno
import htcondor
import os
import subprocess
import sys
import time

from Globals import *
from Utils import Utils

class PersonalCondor(object):


    def __init__(self, name, params=None):
        self._name = name
        self._params = params
        self._master_process = None
        self._is_ready = False
        self._local_dir = name + ".local"
        self._local_path = os.getcwd() + "/" + self._local_dir
        self._local_config = self._local_path + "/condor_config.local"
        self._execute_path = self._local_path + "/execute"
        self._lock_path = self._local_path + "/lock"
        self._log_path = self._local_path + "/log"
        self._run_path = self._local_path + "/run"
        self._spool_path = self._local_path + "/spool"
        self.SetupLocalEnvironment(self._params)
        Utils.TLog("CondorPersonal initialized with path: " + self._local_path)


    def __del__(self):
        self.Stop()


    def Start(self):

        try:
            process = subprocess.Popen(["condor_master", "-f &"])
            if process < 0:
                Utils.TLog("Child was terminated by signal: " + str(process))
                return False
        except OSError as e:
            Utils.TLog("Execution of condor_master failed: " + e)
            return False

        self._master_process = process
        Utils.TLog("Started a new condor_master pid " + str(self._master_process.pid))

        # Wait until we're sure all daemons have started
        self._is_ready = self.WaitForReadyDaemons()
        if self._is_ready is False:
            Utils.TLog("Condor daemons did not enter ready state. Exiting.")
            return False

        Utils.TLog("Condor daemons are active and ready for jobs")

        return True


    def Stop(self):
        if self._master_process is not None:
            Utils.TLog("Shutting down PersonalCondor with condor_off -master")
            os.system("condor_off -master")
            self._master_process = None
        if self._is_ready is True:
            self._is_ready = False


    # Sets up local system environment we'll use to stand up the PersonalCondor instance.
    def SetupLocalEnvironment(self, params=None):

        Utils.MakedirsIgnoreExist(self._local_dir)
        Utils.MakedirsIgnoreExist(self._execute_path)
        Utils.MakedirsIgnoreExist(self._log_path)
        Utils.MakedirsIgnoreExist(self._lock_path)
        Utils.MakedirsIgnoreExist(self._run_path)
        Utils.MakedirsIgnoreExist(self._spool_path)

        # Create a config file in this test's local directory based on existing
        # config settings
        os.system("condor_config_val -write:up " + self._local_config)

        # Add whatever internal config values we need
        config = "LOCAL_DIR = " + self._local_path + "\n"
        config += "EXECUTE = " + self._execute_path + "\n"
        config += "LOCK = " + self._lock_path + "\n"
        config += "LOG = " + self._log_path + "\n"
        config += "RUN = " + self._run_path + "\n"
        config += "SPOOL = " + self._spool_path + "\n"
        config += "COLLECTOR_HOST = $(CONDOR_HOST):0\n"
        config += "MASTER_ADDRESS_FILE = $(LOG)/.master_address\n"
        config += "COLLECTOR_ADDRESS_FILE = $(LOG)/.collector_address\n"
        config += "SCHEDD_ADDRESS_FILE = $(SPOOL)/.schedd_address\n"
        if Utils.IsWindows() is True:
            config += "PROCD_ADDRESS = " + str(htcondor.param["PROCD_ADDRESS"]) + str(os.getpid()) + "\n"

        # Add any custom params
        if params is not None:
            for key in params:
                if params[key] is None:
                    config += key + "\n"
                else:
                    config += key + " = " + str(params[key]) + "\n"

        config_file = open(self._local_config, "a")
        config_file.write(config)
        config_file.close()

        # Set CONDOR_CONFIG to apply the changes we just wrote to file
        self.SetCondorConfig()

        # MRC: What does this function actually do?
        htcondor.reload_config()

        # Now that we have our config setup, delete any old files potentially left over
        Utils.RemoveIgnoreMissing(htcondor.param["MASTER_ADDRESS_FILE"])
        Utils.RemoveIgnoreMissing(htcondor.param["COLLECTOR_ADDRESS_FILE"])
        Utils.RemoveIgnoreMissing(htcondor.param["SCHEDD_ADDRESS_FILE"])


    def SetCondorConfig(self):
        # This is a little confusing, since condor_config.local is something
        # else entirely...
        os.environ["CONDOR_CONFIG"] = self._local_config


    # MRC: Eventually want to do this using python bindings
    def WaitForReadyDaemons(self):
        is_ready_attempts = 6
        for i in range(is_ready_attempts):
            time.sleep(5)
            Utils.TLog("Checking for condor_who output")
            who_output = Utils.RunCondorTool("condor_who -quick -daemon -log " + self._log_path)
            for line in iter(who_output.splitlines()):
                if line == "IsReady = true":
                    return True

        # If we got this far, we never saw the IsReady notice. Return false
        return False
