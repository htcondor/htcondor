#!/usr/bin/env pytest

#-------------------------------------------------------------------------
# Author: Joe Reuss
#
# Notes:
# test shadow hook implementation. Below is a list of the
# tests run:
# 1. Test shadow hook status messages as well as changes to the classad
# 2. Test the shadow hook is run on the shadow side
# 3. Test that the changes a shadow hook makes are reflected in the starter
#       but not the schedd
# 4. Test that errors in shadow hooks are found in the shadow log
# 5. Test that shadow hooks work when the hook keyword is in the
#       config file instead of the submit file
#       NOTE: This test must be the last to run, at least the last job/
#           hook to run since it modifies the config file and the keyword
#           in config will overwrite the other keywords
#--------------------------------------------------------------------------

import os
import stat
from ornithology import *

#===========================================================================
#
# Write out some job hook scripts.
#
@standup
def write_job_hook_scripts(test_dir):
    # Below tests functionality of status codes with shadow hooks:
    script_file = test_dir / "hook_shadow_hold_prepare.sh"
    script_contents = f"""#!/bin/bash
        echo 'HookStatusMessage = "Really bad, going on hold"'
        echo 'HookStatusCode = 190'
        exit 0 """
    script = open(script_file, "w")
    script.write(script_contents)
    script.close()
    st = os.stat(script_file)
    os.chmod(script_file, st.st_mode | stat.S_IEXEC)

    script_file = test_dir / "hook_shadow_idle_prepare.sh"
    script_contents = f"""#!/bin/bash
        echo 'HookStatusMessage = "Kinda bad, will try elsewhere"'
        echo 'HookStatusCode = 390'
        exit 0 """
    script = open(script_file, "w")
    script.write(script_contents)
    script.close()
    st = os.stat(script_file)
    os.chmod(script_file, st.st_mode | stat.S_IEXEC)

    # Test hook is on the shadow side
    # Using a schedd environment variable, we check to see if the shadow hook
    # can access it (since it is on the AP), then to double check we make sure
    # a starter hook can't access it (since it is on the EP).

    # shadow
    filepath = test_dir / "shadow.out"
    script_file = test_dir / "hook_shadow_test_prepare.sh"
    script_contents = f"""#!/bin/bash
        echo $SPECIAL_MSG > {filepath}
        exit 0
        """
    script = open(script_file, "w")
    script.write(script_contents)
    script.close()
    st = os.stat(script_file)
    os.chmod(script_file, st.st_mode | stat.S_IEXEC)

    # starter
    filepath = test_dir / "starter.out"
    # before_transfer starter
    script_file = test_dir / "hook_starter_test_prepare.sh"
    script_contents = f"""#!/bin/bash
        echo $SPECIAL_MSG >> {filepath}
        echo 'HookStatusCode = 190'
        exit 0
        """
    script = open(script_file, "w")
    script.write(script_contents)
    script.close()
    st = os.stat(script_file)
    os.chmod(script_file, st.st_mode | stat.S_IEXEC)


    # Test that the jobad changed at the starter side but not the schedd
    # Do this by using a shadow hook to change a job's classad and by printing
    # contents of .job.ad file into job_ad.out with starter hook

    #shadow hook, used to change a job classad
    job_ad_path = test_dir / "job_ad.out"
    script_file = test_dir / "hook_shadow_check.sh"
    script_contents = f"""#!/bin/bash
        echo 'This is the shadow being the shadow' > {job_ad_path}
        echo 'HookStatusMessage = "Really bad, going on hold"'
        """
    script = open(script_file, "w")
    script.write(script_contents)
    script.close()
    st = os.stat(script_file)
    os.chmod(script_file, st.st_mode | stat.S_IEXEC)

    #starter hook --> used to check change in starter
    script_file = test_dir / "hook_starter_check.sh"
    script_contents = f"""#!/bin/bash
        cat "$_CONDOR_JOB_AD" >> {job_ad_path}
        """
    script = open(script_file, "w")
    script.write(script_contents)
    script.close()
    st = os.stat(script_file)
    os.chmod(script_file, st.st_mode | stat.S_IEXEC)


# Testing shadow hooks when the keyword is defined in the config file rather
# than the submit file
# NOTE: This test must be last (in terms of hooks ran)
    filepath = test_dir / "config_hook.out"
    script_file = test_dir / "hook_config.sh"
    script_contents = f"""#!/bin/bash
    echo 'This message is from the hook!' >> {filepath}
    """
    script = open(script_file, "w")
    script.write(script_contents)
    script.close()
    st = os.stat(script_file)
    os.chmod(script_file, st.st_mode | stat.S_IEXEC)

#==============================================================================

#
# Setup a personal condor with some job hooks defined.
# The "HOLD" hook will put the job on hold.
# The "IDLE" hook will do a shadow exception and put the job back to idle.
# The "TEST" hooks will test to ensure the shadow job hook runs from condor_shadow
# The "CHECK" hooks will test that the changes the hook makes to the classad shows up on
#   the starter but not the schedd
# The "CONFIG" hook will test if the shadow hook works when keyword is in the config file
#   (in this case added in at the end since otherwise it will overwrite the other hookkeywords)
#
@standup
def condor(test_dir, write_job_hook_scripts):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "SHADOW_DEBUG": "D_FULLDEBUG",
            "SCHEDD_ENVIRONMENT" : "SPECIAL_MSG='This message is brought to you by the shadow!'",
            "HOLD_HOOK_SHADOW_PREPARE_JOB" : test_dir / "hook_shadow_hold_prepare.sh",
            "IDLE_HOOK_SHADOW_PREPARE_JOB" : test_dir / "hook_shadow_idle_prepare.sh",
            "TEST_HOOK_SHADOW_PREPARE_JOB" : test_dir / "hook_shadow_test_prepare.sh",
            "TEST_HOOK_PREPARE_JOB_BEFORE_TRANSFER" : test_dir / "hook_starter_test_prepare.sh",
            "CHECK_HOOK_PREPARE_JOB" : test_dir / "hook_starter_check.sh",
            "CHECK_HOOK_SHADOW_PREPARE_JOB" : test_dir / "hook_shadow_check.sh",
            "CONFIG_HOOK_SHADOW_PREPARE_JOB" : test_dir / "hook_config.sh",
        }
    ) as condor:
        yield condor


#
# Submit a job using hook config (from config file)
#
@action
def submit_configjob(test_dir, condor, path_to_sleep):
    reconfig(condor) # run reconfig (add keyword to config file)
    config_submit = condor.submit(
        description={"executable": path_to_sleep,
            "arguments": "0",
            "log": "config_hook_job_events.log",
        }
    )
    config_submit.wait(condition=ClusterState.any_running,timeout=30)
    return config_submit.query()

#
# Submit a job using the hook check
#
@action
def submit_checkjob(condor, path_to_sleep):
    return condor.submit(
        description={"executable": path_to_sleep,
                "arguments": "0",
                "+HookKeyword" : '"check"',
                "log": "check_hook_job_events.log",
            }
    )

@action
def checkjob_starter(submit_checkjob):
    return submit_checkjob.query()

#
# Submit a job using the hook test.
#
@action
def submit_testjob(condor, path_to_sleep):
    return condor.submit(
            description={"executable": path_to_sleep,
                "arguments": "0",
                "+HookKeyword" : '"test"',
                "log": "test_hook_job_events.log",
            }
    )

@action
def testjob(submit_testjob):
    # Wait for job to go on hold
    assert submit_testjob.wait(condition=ClusterState.any_held,timeout=60)
    # Return the first (and only) job ad in the cluster for testing class to reference
    return submit_testjob.query()[0]

#
# Submit a job using the hook hold.  This job should end up on hold.
#
@action
def submit_heldjob(condor, path_to_sleep):
    return condor.submit(
            description={"executable": path_to_sleep,
                "arguments": "0",
                "+HookKeyword" : '"hold"',
                "log": "hold_hook_shadow_job_events.log"
                }
    )

@action
def heldjob(submit_heldjob):
    # Wait for job to go on hold
    assert submit_heldjob.wait(condition=ClusterState.any_held,timeout=60)
    # Return the first (and only) job ad in the cluster for testing class to reference
    return submit_heldjob.query()[0]

#
# Submit a job using the "idle" hooks.  This job should start running, then go
# back to idle before exectution.  Add a requirement so it only tries to start once.
#
@action
def submit_idlejob(condor, path_to_sleep):
    return condor.submit(
            description={"executable": path_to_sleep,
                "requirements" : "NumShadowStarts =!= 1",
                "arguments": "0",
                "+HookKeyword" : '"idle"',
                "log": "idle_hook_shadow_job_events.log"
                }
    )

@action
def idlejob(condor,submit_idlejob):
    jobid = submit_idlejob.job_ids[0]
    assert condor.job_queue.wait_for_events(
            expected_events={jobid: [
                SetJobStatus(JobStatus.RUNNING),
                SetJobStatus(JobStatus.IDLE),
                ]},
            timeout=60
            )
    return submit_idlejob.query()[0]

# get the shadow log to ensure errors are recorded there
@action
def shadow_job_log(condor, heldjob, idlejob):
    shadow_log_filepath = condor.shadow_log
    return shadow_log_filepath

# function to query the schedd
@action
def checkjob_schedd(condor, submit_checkjob):
    # wait for the job to run
    submit_checkjob.wait(condition=ClusterState.any_running,timeout=30)
    # Query the schedd
    schedd = Condor.query(self=condor, projection=['HookStatusMessage'])
    return schedd

def reconfig(htcondor):
    p = htcondor.run_command(["condor_config_val","LOCAL_DIR"])
    config = os.path.join(p.stdout, "condor_config")
    with open(config, "a") as f:
        f.write("\nSHADOW_JOB_HOOK_KEYWORD = CONFIG\n")

class TestShadowHook:
    # Methods that begin with test_* are tests.

    # Test codes with shadow hooks (and updates to job ads)
    def test_holdreasoncode(self, heldjob):
        assert heldjob["HoldReasonCode"] == 48 #48 means HookShadowPrepareJobFailure (we want that)

    def test_holdreasonsubcode(self, heldjob):
        assert heldjob["HoldReasonSubCode"] == 190

    def test_holdreasonsubcode(self, testjob):
        assert testjob["HoldReasonSubCode"] == 190

    # Test to ensure jobad changes on the starter side but not the schedd

    # Check not in schedd by querying the schedd
    def test_not_in_schedd(self, checkjob_schedd):
        assert 'Really bad, going on hold' not in checkjob_schedd

    # Use hooks to check the status message changed in starter
    def test_in_starter(self, checkjob_starter):
        with open('job_ad.out') as f:
            log = f.read()
            assert 'Really bad, going on hold' in log

    # Check to see that the shadow hook spawns from the shadow and runs before a starter hook
    def test_from_shadow(self, testjob):
        with open('shadow.out') as f:
            log = f.read()
            #assert 'shadows!' in log
            assert "This message is brought to you by the shadow!" in log
        with open('starter.out') as f:
            log = f.read()
            assert "This message is brought to you by the shadow!" not in log

    # Testing of status messages, see job classad changed:
    def test_hold_reason(self, heldjob):
        assert "Really bad" in heldjob["HoldReason"]

    def test_hold_status_msg(self, heldjob):
        with open('hold_hook_shadow_job_events.log') as f:
            log = f.read()
            assert 'Really bad' in log

    def test_idle_status(self, idlejob):
        assert idlejob["NumShadowStarts"] == 1
        assert idlejob["NumJobStarts"] == 0
        assert idlejob["JobStatus"] == 1

    def test_idle_status_msg(self, idlejob):
        with open('idle_hook_shadow_job_events.log') as f:
            log = f.read()
            assert 'Kinda bad' in log

    def test_hold_numholdsbyreason_was_policy(self, heldjob):
        assert dict(heldjob["NumHoldsByReason"]) == { 'HookShadowPrepareJobFailure' : 1 }

    # Test that shadow hooks work when keyword is in the config file:
    def test_keyword_in_config(self, submit_configjob):
        with open ('config_hook.out') as f:
            log = f.read()
            assert 'This message is from the hook!' in log

    # find that the error messages are in shadow log
    def test_in_shadow_log(self, shadow_job_log):
        f = shadow_job_log.open()
        log = f.read()
        found_messages = {"idle" : False, "held" : False}
        for line in log:
            if 'Really bad' in line:
                found_messages["held"] = True
            elif 'Kinda bad' in line:
                found_messages["idle"] = True
            if found_messages["held"] == True and found_messages["idle"] == True:
                break
        assert found_messages["held"] == True
        assert found_messages["idle"] == True

