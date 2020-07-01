#!/usr/bin/env pytest

#
# Can we start a backfill job while the startd is draining?
#

#
# FIXME: this test should but does not duplicate cmd_drain_scavenging and
# cmd_drain_scavenging_vacation.
#

#
# If we didn't remove the jobs during tear-down, Ornithology would have to
# wait for its whole shutdown time-out, because HTCondor will be waiting
# for MAX_JOB_RETIREMENT_TIME.  That saves us ~40 seconds on a test that
# otherwise takes ~56 seconds.
#

import time
import logging
import htcondor
import textwrap

from ornithology import (
    config,
    standup,
    action,
    JobStatus,
    ClusterState,
    Condor,
)


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


# This layer of indirection may not be necessary if we're not parameterizing.
@config
def condor_config():
    config = {
        "STARTD_DEBUG": "D_SUB_SECOND D_FULLDEBUG D_JOB",
        "MAXJOBRETIREMENTTIME": 300,
    }
    raw_config = textwrap.dedent(
        """
        use feature : PartitionableSlot
        use policy : HOLD_IF_MEMORY_EXCEEDED
    """
    )
    return {"config": config, "raw_config": raw_config}


@standup
def condor(condor_config, test_dir):
    with Condor(test_dir / "condor", **condor_config) as condor:
        yield condor


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


@action
def job(condor, job_parameters):
    job = condor.submit(job_parameters, count=1)

    yield job

    job.remove()


def issue_drain_command(condor):
    with condor.use_config():
        hostname = htcondor.param["FULL_HOSTNAME"]

    rv = condor.run_command(["condor_drain", "-start", "IsBackfill == true", hostname])
    assert rv.returncode == 0


def wait_for_draining_to_start(condor):
    collector = condor.get_local_collector()
    for delay in range(1, 20):
        time.sleep(1)
        ads = collector.query(
            htcondor.AdTypes.Startd, True, ["Name", "State", "Activity"]
        )

        foundClaimedRetiring = False
        for ad in ads:
            if ad["Name"].startswith("slot1@"):
                continue
            if ad["State"] == "Claimed" and ad["Activity"] == "Retiring":
                foundClaimedRetiring = True
                break

        if foundClaimedRetiring:
            break

    assert foundClaimedRetiring


@action
def backfill_job(condor, job, job_parameters):
    assert job.wait(
        condition=ClusterState.all_running,
        timeout=60,
        fail_condition=ClusterState.any_terminal,
    )

    issue_drain_command(condor)
    wait_for_draining_to_start(condor)

    job_parameters["+IsBackfill"] = "true"
    backfill_job = condor.submit(job_parameters, count=1)

    yield backfill_job

    backfill_job.remove()


class TestJob:
    def test_job_starts(self, backfill_job):
        assert backfill_job.wait(
            condition=ClusterState.all_running,
            timeout=60,
            fail_condition=ClusterState.any_held,
        )
