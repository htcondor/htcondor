#!/usr/bin/env pytest

#
# Does HOLD_IF_MEMORY_EXCEEDED work on backfill-while-draining jobs?
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
    backfill_job,

    ask_to_misbehave,
)

@action
def misbehaving_backfill_job(backfill_job, kill_file):
    assert backfill_job.wait(
        condition=ClusterState.all_running,
        timeout=60,
        fail_condition=ClusterState.any_terminal,
    )

    ask_to_misbehave(backfill_job, 0, kill_file)
    return backfill_job


class TestJob:
    def test_misbehaving_backfill_job_held(self, misbehaving_backfill_job):
        assert misbehaving_backfill_job.wait(
            condition=ClusterState.all_held,
            timeout=60,
            fail_condition=lambda self: self.any_status(
                JobStatus.COMPLETED, JobStatus.REMOVED
            ),
        )
