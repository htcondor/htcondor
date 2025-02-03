#!/usr/bin/env pytest

import os
import stat
from ornithology import *

#
# Write out some simple job hook scripts.
#
@standup
def write_job_hook_scripts(test_dir):
    script_file = test_dir / "hook_prepare_before.sh"
    script_contents = f"""#!/bin/bash
echo 'HookStatusMessage = "So far so good buddy"'
exit 0 """
    script = open(script_file, "w")
    script.write(script_contents)
    script.close()
    st = os.stat(script_file)
    os.chmod(script_file, st.st_mode | stat.S_IEXEC)

    script_file = test_dir / "hook_hold_prepare.sh"
    script_contents = f"""#!/bin/bash
echo 'HookStatusMessage = "Really bad, going on hold"'
echo 'HookStatusCode = 190'
exit 0 """
    script = open(script_file, "w")
    script.write(script_contents)
    script.close()
    st = os.stat(script_file)
    os.chmod(script_file, st.st_mode | stat.S_IEXEC)

    script_file = test_dir / "hook_idle_prepare.sh"
    script_contents = f"""#!/bin/bash
echo 'HookStatusMessage = "Kinda bad, will try elsewhere"'
echo 'HookStatusCode = 390'
exit 0 """
    script = open(script_file, "w")
    script.write(script_contents)
    script.close()
    st = os.stat(script_file)
    os.chmod(script_file, st.st_mode | stat.S_IEXEC)

#
# Setup a personal condor with some job hooks defined.
# The "HOLD" hooks will put the job on hold.
# The "IDLE" hooks will do a shadow exception and put the job back to idle.
#
@standup
def condor(test_dir, write_job_hook_scripts):
    with Condor(
        local_dir=test_dir / "condor", 
        config={
            "STARTER_DEBUG": "D_FULLDEBUG",
            "STARTER_DEFAULT_JOB_HOOK_KEYWORD" : "hold",
            "HOLD_HOOK_PREPARE_JOB_BEFORE_TRANSFER" : test_dir / "hook_prepare_before.sh",
            "IDLE_HOOK_PREPARE_JOB_BEFORE_TRANSFER" : test_dir / "hook_prepare_before.sh",
            "HOLD_HOOK_PREPARE_JOB" : test_dir / "hook_hold_prepare.sh",
            "IDLE_HOOK_PREPARE_JOB" : test_dir / "hook_idle_prepare.sh"
        }
    ) as condor:
        yield condor

#
# Submit a job using the default hook (hold).  This job
# should end up on hold.
#
@action
def submit_heldjob(condor, path_to_sleep):
    return condor.submit(
            description={"executable": path_to_sleep,
                "arguments": "0",
                "log": "hold_hook_job_events.log"
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
                "log": "idle_hook_job_events.log"
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

class TestHookStatusCodeAndMsg:
    # Methods that begin with test_* are tests.

    def test_holdreasoncode(self, heldjob):
        assert heldjob["HoldReasonCode"] == 19

    def test_holdreasonsubcode(self, heldjob):
        assert heldjob["HoldReasonSubCode"] == 190

    def test_hold_numholdsbyreason_was_policy(self, heldjob):
        assert dict(heldjob["NumHoldsByReason"]) == { 'HookPrepareJobFailure' : 1 }

    def test_hold_reason(self, heldjob):
        assert "Really bad" in heldjob["HoldReason"]

    def test_hold_status_msg(self, heldjob):
        with open('hold_hook_job_events.log') as f:
            log = f.read()
            assert 'So far so good' in log
            assert 'Really bad' in log

    def test_idle_status(self, idlejob):
        assert idlejob["NumShadowStarts"] == 1
        assert idlejob["NumJobStarts"] == 0
        assert idlejob["JobStatus"] == 1

    def test_idle_status_msg(self, idlejob):
        with open('idle_hook_job_events.log') as f:
            log = f.read()
            assert 'So far so good' in log
            assert 'Kinda bad' in log

