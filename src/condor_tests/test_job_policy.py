#!/usr/bin/env pytest

#
# Does HOLD_IF_MEMORY_EXCEEDED work during normal (non-draining) operation?
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
)


@action
def misbehaving_job(job, kill_file):
    assert job.wait(
        condition=ClusterState.all_running,
        timeout=60,
        fail_condition=ClusterState.any_terminal,
    )

    ask_to_misbehave(job, 0, kill_file)
    return job


class TestHoldIfMemoryExceeded:
    def test_job_is_held(self, misbehaving_job):
        assert misbehaving_job.wait(
            condition=ClusterState.all_held,
            timeout=60,
            fail_condition=lambda self: self.any_status(
                JobStatus.COMPLETED, JobStatus.REMOVED
            ),
        )
