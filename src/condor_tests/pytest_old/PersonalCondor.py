from __future__ import absolute_import

import os
import re
import subprocess
import time

import classad
import htcondor

from .Globals import *
from .Utils import Utils
from .CondorCluster import CondorCluster

class PersonalCondor(object):

    # For internal use only.  Use CondorTest.StartPersonalCondor(), instead.
    def __init__(self, name, params=None, ordered_params=None):
        self._name = name
        self._params = params
        self._ordered_params = ordered_params
        self._master_process = None
        self._is_ready = False
        self._local_dir = name + ".local"
        self._local_path = os.getcwd() + "/" + self._local_dir
        self._local_config = self._local_path + "/condor_config"
        self._execute_path = self._local_path + "/execute"
        self._lock_path = self._local_path + "/lock"
        self._log_path = self._local_path + "/log"
        self._run_path = self._local_path + "/run"
        self._spool_path = self._local_path + "/spool"
        self._SetupLocalEnvironment()
        Utils.TLog("CondorPersonal initialized with path: " + self._local_path)
        self._schedd = None

    # @return True iff this persona condor is ready.
    def __bool__(self):
        return self._is_ready
    __nonzero__ = __bool__


    #
    # Return objects which are or use one of our daemons.
    #

    def CondorCluster(self, job_args):
        if self._schedd is None:
            try:
                original = self.SetCondorConfig()
                htcondor.reload_config()

                name = htcondor.param["SCHEDD_ADDRESS_FILE"]
                f = open( name, 'r' )
                contents = f.read()
                f.close()

                c = classad.ClassAd()
                (address, version, platform) = contents.splitlines()
                c["MyAddress"] = address
                c["Name"] = "Unknown"
                c["CondorVersion"] = version
                # Utils.TLog( "[PC: {0}] Constructing schedd from address '{1}' with version '{2}'".format(self._name, address, version))
                self._schedd = htcondor.Schedd(c)
            except IOError as ioe:
                # Utils.TLog( "[PC: {0}] Constructing default schedd because of IOError {1}".format(self._name, str(ioe)))
                self._schedd = htcondor.Schedd()
            finally:
                os.environ["CONDOR_CONFIG"] = original
                htcondor.reload_config()
        return CondorCluster(job_args, self._schedd)


    #
    # Process control.  Waiting for a personal condor to shut down can take
    # quite a while, so we let callers split the process up if they've got
    # more than one.
    #

    def BeginStopping(self):
        if self._master_process is not None:
            Utils.TLog("[PC: {0}] Shutting down PersonalCondor with condor_off -master".format(self._name))
            r = Utils.RunCommandCarefully( ( 'condor_off', '-master' ), 20 )
            if not r:
                Utils.TLog("[PC: {0}] condor_off -master failed, using terminate()".format(self._name))
                self._master_process.terminate()
            self._is_ready = False

    def FinishStopping(self):
        then = time.time()
        while time.time() - then < 20:
            if self._master_process.poll() is None:
                Utils.TLog("[PC: {0}] Master did not exit, will check again in five seconds...".format(self._name))
                time.sleep(5)
            else:
                self._master_process = None
                Utils.TLog("[PC: {0}] Master exited".format(self._name))
                return

        Utils.TLog("[PC: {0}] Master did not exit of its own accord, trying to terminate it".format(self._name))
        self._master_process.terminate()
        then = time.time()
        while time.time() - then < 20:
            if self._master_process.poll() is None:
                Utils.TLog("[PC: {0}] Master did not exit, will check again in five seconds...".format(self._name))
                time.sleep(5)
            else:
                self._master_process = None
                Utils.TLog("[PC: {0}] Master exited".format(self._name))
                return

        #
        # kill() and terminate() are synonymous on Windows.  We could also try
        # send_signal() either subprocess.CTRL_C_EVENT or .CTRL_BREAK_EVENT,
        # if we're on Windows and we created the process the right way.
        #
        Utils.TLog("[PC: {0}] Master did not exit after being terminated, calling security".format(self._name))
        self._master_process.kill()
        then = time.time()
        while time.time() - then < 5:
            if self._master_process.poll() is None:
                Utils.TLog("[PC: {0}] Master did not exit, will check again in a second...".format(self._name))
                time.sleep(1)
            else:
                self._master_process = None
                Utils.TLog("[PC: {0}] Master exited".format(self._name))
                return

        # Arguably, this should be a test failure.
        Utils.TLog("[PC: {0}] Master was kill()ed but did not exit, giving up.".format(self._name))

    def Stop(self):
        self.BeginStopping()
        self.FinishStopping()

    def Start(self):
        try:
            process = subprocess.Popen(["condor_master", "-f"])
            if not process:
                Utils.TLog("[PC: {0}] Child was terminated by signal: {1}".format(self._name, str(process)))
                return False
        except OSError as e:
            Utils.TLog("[PC: {0}] Execution of condor_master failed: {1}".format(self._name, str(e)))
            return False

        self._master_process = process
        Utils.TLog("[PC: {0}] Started a new condor_master pid {1}".format( self._name, str(self._master_process.pid)))

        # Wait until we're sure all daemons have started
        self._is_ready = self._WaitForReadyDaemons()
        if self._is_ready is False:
            Utils.TLog("[PC: {0}] Condor daemons did not enter ready state. Exiting.".format(self._name))
            return False

        Utils.TLog("[PC: {0}] Condor daemons are active and ready for jobs".format(self._name))
        return True

    default_params = {
        "UPDATE_INTERVAL" : 5,
        "POLLING_INTERVAL" : 5,
        "NEGOTIATOR_INTERVAL" : 5,
        "STARTER_UPDATE_INTERVAL" : 5,
        "STARTER_INITIAL_UPDATE_INTERVAL" : 5,
        "NEGOTIATOR_CYCLE_DELAY" : 5,
        "MachineMaxVacateTime" : 5,
        "MAIL" : "/bin/true",
        "SENDMAIL" : "/bin/true",
    }

    # Sets up local system environment we'll use to stand up the PersonalCondor instance.
    # For internal use only.
    def _SetupLocalEnvironment(self):
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
        config = """
#
# From PersonalCondor
#
"""
        # `condor_config_val -write:up` includes the local config file.  Don't
        # include it again, since the right thing to set this knob to is
        # '$(LOCAL_DIR)/condor_config.local', and that file won't exist.
        config += "LOCAL_CONFIG_FILE = \n"
        config += "LOCAL_DIR = " + self._local_path + "\n"
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
            # This call to htcondor.param() will return the correct value iff
            # nobody set CONDOR_CONFIG without calling htcondor.reload_config();
            # it's not clear if it's better for us to call that before calling
            # condor_config_val above, or if to avoid perturbing the system
            # any more than necessary.
            config += "PROCD_ADDRESS = " + str(htcondor.param["PROCD_ADDRESS"]) + str(os.getpid()) + "\n"
        config += """
#
# Default params
#
"""
        for key in PersonalCondor.default_params:
            config += key + " = " + str(PersonalCondor.default_params[key]) + "\n"

        # Add any custom params
        if self._params is not None:
            config += """
#
# Custom params
#
"""
            for key in self._params:
                if self._params[key] is None:
                    config += key + "\n"
                else:
                    config += key + " = " + str(self._params[key]) + "\n"

        if self._ordered_params is not None:
            config += """
#
# Ordered params
#
"""
            config += self._ordered_params

        config_file = open(self._local_config, "a")
        config_file.write(config)
        config_file.close()

        # Set CONDOR_CONFIG to apply the changes we just wrote to file
        self.SetCondorConfig()

		# If we didn't do this, htcondor.param[] would return results from the
		# old CONDOR_CONFIG, which most would find astonishing (since most of
		# the time, there will only be a single relevant instance).
        htcondor.reload_config()

        # Now that we have our config setup, delete any old files potentially left over
        Utils.RemoveIgnoreMissing(htcondor.param["MASTER_ADDRESS_FILE"])
        Utils.RemoveIgnoreMissing(htcondor.param["COLLECTOR_ADDRESS_FILE"])
        Utils.RemoveIgnoreMissing(htcondor.param["SCHEDD_ADDRESS_FILE"])

    def SetCondorConfig(self):
        previous_condor_config = os.environ.get("CONDOR_CONFIG")
        os.environ["CONDOR_CONFIG"] = self._local_config
        return previous_condor_config

    # TODO: Eventually want to do this using python bindings
    # For internal use only.
    def _WaitForReadyDaemons(self):
        # 120 seconds to start a personal condor seems like a lot
        # but it matches the years of experience in the perl tests
        is_ready_attempts = 24
        for i in range(is_ready_attempts):
            time.sleep(5)
            Utils.TLog("[PC: {0}] Checking for condor_who output".format(self._name))
            who_output = Utils.RunCondorTool("condor_who -quick -daemon -log " + self._log_path)
            for line in iter(who_output.splitlines()):
                if line == "IsReady = true":
                    return True

        # If we got this far, we never saw the IsReady notice. Return false
        return False

