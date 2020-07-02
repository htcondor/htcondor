#!/usr/bin/env pytest

#
# Does HOLD_IF_MEMORY_EXCEEDED work on jobs while the startd is draining?
#

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

from ornithology import (
    action,
    JobStatus,
    ClusterState,
)

from job_policy_testing import (
    condor_config,
    condor,
    job_file,
    kill_file,
    job_parameters,
    job,

    ask_to_misbehave,
    issue_drain_command,
    wait_for_draining_to_start,
)


@action
def misbehaving_draining_job(condor, job, kill_file):
    assert job.wait(
        condition=ClusterState.all_running,
        timeout=60,
        fail_condition=ClusterState.any_terminal,
    )

    issue_drain_command(condor)
    wait_for_draining_to_start(condor)

    ask_to_misbehave(job, 0, kill_file)
    return job


class TestJob:
    def test_misbehaving_job_held(self, misbehaving_draining_job):
        assert misbehaving_draining_job.wait(
            condition=ClusterState.all_held,
            timeout=60,
            fail_condition=lambda self: self.any_status(
                JobStatus.COMPLETED, JobStatus.REMOVED
            ),
        )
