import classad
import errno
import htcondor
import os
import subprocess
import sys
import time

from CondorTest import CondorTest


class PersonalCondor(object):


    def __init__(self, name):
        self._name = name
        self._local_dir = name + ".local"
        self._local_path = os.getcwd() + "/" + self._local_dir
        self._local_config = self._local_path + "/condor_config.local"
        self._execute_path = self._local_path + "/execute"
        self._lock_path = self._local_path + "/lock"
        self._log_path = self._local_path + "/log"
        self._run_path = self._local_path + "/run"
        self._spool_path = self._local_path + "/spool"
        self.SetupLocalEnvironment()
        CondorTest.TLog("CondorPersonal initialized with path: " + self._local_path)


    def __del__(self):
        self.Stop()


    def Start(self):

        try:
            p = subprocess.Popen(["condor_master", "-f &"])
            if p < 0:
                CondorTest.TLog("Child was terminated by signal: " + str(p))
                return False
        except OSError as e:
            CondorTest.TLog("Execution of condor_master failed: " + e)
            return False

        self._master_p = p

        # Wait until we're sure all daemons have started
        is_ready = self.WaitForReadyDaemons()
        if is_ready is False:
            CondorTest.TLog("Condor daemons did not enter reaedy state. Exiting.")
            return False

        CondorTest.TLog("Started a new personal condor with condor_master pid " + str(self._master_p.pid))

        return True


    def Stop(self):
        CondorTest.TLog("Shutting down PersonalCondor with condor_off -master")
        os.system("condor_off -master")


    # Sets up local system environment we'll use to stand up the PersonalCondor instance.
    def SetupLocalEnvironment(self):

        CondorTest.MakedirsIgnoreExist(self._local_dir)
        CondorTest.MakedirsIgnoreExist(self._execute_path)
        CondorTest.MakedirsIgnoreExist(self._log_path)
        CondorTest.MakedirsIgnoreExist(self._lock_path)
        CondorTest.MakedirsIgnoreExist(self._run_path)
        CondorTest.MakedirsIgnoreExist(self._spool_path)

        CondorTest.RemoveIgnoreMissing(htcondor.param["MASTER_ADDRESS_FILE"])
        CondorTest.RemoveIgnoreMissing(htcondor.param["COLLECTOR_ADDRESS_FILE"])
        CondorTest.RemoveIgnoreMissing(htcondor.param["SCHEDD_ADDRESS_FILE"])

        # Create a config file in this test's local directory based on existing
        # config settings
        os.system("condor_config_val -write:up " + self._local_config)

        # Add whatever internal config values we need
        config = "LOCAL_DIR = " + self._local_path + "\n"
        config += "EXECUTE = " + self._execute_path + "\n"
        config += "LOCK = " + self._lock_path + "\n"
        config += "RUN = " + self._run_path + "\n"
        config += "SPOOL = " + self._spool_path + "\n"
        config += "COLLECTOR_HOST = $(CONDOR_HOST):0\n"
        
        config_file = open(self._local_config, "a")
        config_file.write(config)
        config_file.close()
        
        os.environ["CONDOR_CONFIG"] = self._local_config

        # MRC: What does this function actually do?
        htcondor.reload_config()


    # MRC: Eventually want to do this using python bindings
    def WaitForReadyDaemons(self):
        is_ready_attempts = 6
        for i in range(is_ready_attempts):
            time.sleep(5)
            CondorTest.TLog("Checking for condor_who output")
            who_output = CondorTest.RunCondorTool("condor_who -quick -daemon -log " + self._log_path)
            for line in iter(who_output.splitlines()):
                if line == "IsReady = true":
                    return True

        # If we got this far, we never saw the IsReady notice. Return false
        return False
