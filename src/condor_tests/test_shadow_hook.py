#!/usr/bin/env pytest

#---------------------------------------------------------------
# Author: Joe Reuss
#
# Notes:
# test shadow hook implementation. Checks if hook works by changing job classAd status messages
# Also tested to ensure it runs on the shadow and not the starter. Two job hooks are created to
# ensure the shadow hook runs before the starter (see below)
#---------------------------------------------------------------

import os
import stat
from ornithology import *


#
# Write out some simple shadow job hook scripts.
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

    # Test job is on the shadow side
    # Have 2 hooks, 1 shadow hook that runs before the files are
    # transferred and run by the shadow and 1 hook_prepare_job_before_transfer
    # that is also before files are transferred but ran by the starter.
    # Should show that shadow will always be before the starter

    # shadow
    filepath = test_dir / "test.out"
    script_file = test_dir / "hook_shadow_test_prepare.sh"
    script_contents = f"""#!/bin/bash
        echo 'From the shadows!' > {filepath}
        exit 0
        """
    script = open(script_file, "w")
    script.write(script_contents)
    script.close()
    st = os.stat(script_file)
    os.chmod(script_file, st.st_mode | stat.S_IEXEC)

    # before_transfer starter
    script_file = test_dir / "hook_starter_test_prepare.sh"
    script_contents = f"""#!/bin/bash
        echo 'From the starter!' >> {filepath}
        echo 'HookStatusCode = 190'
        exit 0
        """
    script = open(script_file, "w")
    script.write(script_contents)
    script.close()
    st = os.stat(script_file)
    os.chmod(script_file, st.st_mode | stat.S_IEXEC)




#
# Setup a personal condor with some job hooks defined.
# The "HOLD" hook will put the job on hold.
# The "IDLE" hook will do a shadow exception and put the job back to idle.
# The "TEST" hooks will test to ensure the shadow job hook performs before the starter
#
@standup
def condor(test_dir, write_job_hook_scripts):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "STARTER_DEBUG": "D_FULLDEBUG",
            "HOLD_HOOK_SHADOW_PREPARE_JOB" : test_dir / "hook_shadow_hold_prepare.sh",
            "IDLE_HOOK_SHADOW_PREPARE_JOB" : test_dir / "hook_shadow_idle_prepare.sh",
            "TEST_HOOK_SHADOW_PREPARE_JOB" : test_dir / "hook_shadow_test_prepare.sh",
            "TEST_HOOK_PREPARE_JOB_BEFORE_TRANSFER" : test_dir / "hook_starter_test_prepare.sh",

            #"FUNC_HOOK_SHADOW_PREPARE_JOB" : test_dir / "hook_shadow_func_prepare.sh",
        }
    ) as condor:
        yield condor


#
# Submit a job using the hook test.
#
@action
def submit_testjob(condor, path_to_sleep):
    return condor.submit(
            description={"executable": path_to_sleep,
                "arguments": "0",
                "+HookKeyword" : '"test"',
                "log": "test_hook_job_events.log"
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
    # Return the first (and only) job ad in the cluster for testing class to reference
    return submit_idlejob.query()[0]



class TestShadowHook:
    # Methods that begin with test_* are tests.

    #Testing of status messages:
    def test_holdreasoncode(self, heldjob):
        assert heldjob["HoldReasonCode"] == 48 #48 means HookShadowPrepareJobFailure (we want that)

    def test_holdreasonsubcode(self, heldjob):
        assert heldjob["HoldReasonSubCode"] == 190

    def test_holdreasonsubcode(self, testjob):
        assert testjob["HoldReasonSubCode"] == 190

    def test_shadowbeforestarter(self, testjob):
        with open('test.out') as f:
            log = f.read()
            assert 'shadows!' in log
            assert 'starter!' in log

    def test_hold_numholdsbyreason_was_policy(self, heldjob):
        assert heldjob["NumHoldsByReason"] == { 'HookShadowPrepareJobFailure' : 1 }

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



