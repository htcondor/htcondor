#!/usr/bin/env pytest

#
# Can we start a backfill job while the startd is draining?
#

#
# FIXME: this test should but does not duplicate cmd_drain_scavenging and
# cmd_drain_scavenging_vacation.
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
    job,
    backfill_job,

    ask_to_misbehave,
)


@action
def job_parameters(path_to_sleep):
    return {
        "executable": path_to_sleep,
        "transfer_executable": "true",
        "should_transfer_files": "true",
        "universe": "vanilla",
        "arguments": "3600",
        "log": "cmd_drain_policies.log",
    }


class TestJob:
    def test_job_starts(self, backfill_job):
        assert backfill_job.wait(
            condition=ClusterState.all_running,
            timeout=60,
            fail_condition=ClusterState.any_held,
        )
