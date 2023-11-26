#!/usr/bin/env pytest

import logging
import os
#from pathlib import Path

from ornithology import (
    config,
    standup,
    action,
    Condor,
    ClusterState,
    JobStatus
)


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

@standup
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "SUBMIT_REQUIREMENT_NAMES": "flag",
            "SUBMIT_REQUIREMENT_flag": "flag == true"
        },
    ) as condor:
        yield condor

@action
def submit_req_job(condor):
    job = condor.submit(
        {
            "executable": "/bin/sleep",
            "arguments":  "0",
            "log": (condor.local_dir / "srt.log").as_posix(),
            "MY.flag": "true",
        }
    )
    assert job.wait(condition=ClusterState.all_complete, timeout=60)
    return job

@action
def submit_req_job_fails(condor):
    result = False
    try:
        schedd = condor.get_local_schedd()
        jdl = htcondor.Submit(
            """
            executable = /bin/sleep
            arguments = 0
            log = srt2.log
            My.flag = false
            """
        )
        job = schedd.submit(jdl)
    except Exception as e:
        result = True
    return result


class TestSubmitRequirements:
    def test_submit_requirements_job_succeeds(self, condor, submit_req_job):
        assert submit_req_job.state[0] == JobStatus.COMPLETED
    def test_submit_requirements_job_fails(self, condor, submit_req_job_fails):
        assert submit_req_job_fails
