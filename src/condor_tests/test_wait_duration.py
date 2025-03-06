#!/usr/bin/env pytest


# Test that the job attribute InitialWaitDuration 
# is set, and to a reasonable value

import logging

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

@standup
def condor(test_dir):
    with Condor(test_dir / "condor") as condor:
        yield condor

@action
def completed_test_job(condor):
    completed_test_job = condor.submit(
        {
            "executable":           "/bin/sleep",
            "arguments":            "0",
            "log":                  "log",
            "leave_in_queue":       "true",
        }
    )

    assert completed_test_job.wait(
        condition=ClusterState.all_terminal,
        timeout=300,
    )
    return completed_test_job

class TestWaitDuration:
    def test_job(self, completed_test_job):

        the_job_ad = completed_test_job.query()[0]
        wait_time = the_job_ad["InitialWaitDuration"]
        assert (wait_time >= 0) & (wait_time < 300)
