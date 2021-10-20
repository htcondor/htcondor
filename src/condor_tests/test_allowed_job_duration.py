#!/usr/bin/env pytest

import htcondor

from ornithology import (
    standup,
    Condor,
    action,
    in_order,
    ClusterState,
    SetJobStatus,
    JobStatus,
    SetAttribute,
)

import logging
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

ALLOWED_JOB_DURATION = 20
PERIODIC_EXPR_INTERVAL = 5


@standup
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor",
        config={"PERIODIC_EXPR_INTERVAL": PERIODIC_EXPR_INTERVAL}
    ) as condor:
        yield condor


@action
def test_job_handle(condor, path_to_sleep, test_dir):
    handle = condor.submit(
        description={
            "executable": path_to_sleep,
            "arguments": 2 * (ALLOWED_JOB_DURATION + PERIODIC_EXPR_INTERVAL + 1),
            "transfer_executable": False,
            "should_transfer_files": True,
            "universe": "vanilla",
            "log": test_dir / "test_job.log",
            "on_exit_remove": False,
            "allowed_job_duration": ALLOWED_JOB_DURATION,
        },
        count=1
    )

    job_id = handle.job_ids[0]
    timeout = 4 * (ALLOWED_JOB_DURATION + PERIODIC_EXPR_INTERVAL + 1)

    handle.wait(
        condition=ClusterState.any_held,
        fail_condition=ClusterState.any_complete,
        verbose=True,
        timeout=timeout
    )
    condor.job_queue.wait_for_events(
        expected_events={job_id: [SetJobStatus(JobStatus.HELD)]},
        unexpected_events={job_id: [SetJobStatus(JobStatus.COMPLETED)]},
        timeout=timeout
    )

    yield handle

    handle.remove()


@action
def test_job_log_events(test_job_handle):
    return test_job_handle.event_log.filter(lambda event: True)


@action
def test_job_queue_events(condor, test_job_handle):
    job_id = test_job_handle.job_ids[0]
    return condor.job_queue.by_jobid[job_id]


class TestAllowedJobDuration:
    def test_allowed_job_duration_sequence(self, test_job_queue_events):
        assert in_order(
            test_job_queue_events,
            [
                SetJobStatus(JobStatus.IDLE),
                SetJobStatus(JobStatus.RUNNING),
                SetJobStatus(JobStatus.HELD),
            ]
        )


    def test_allowed_job_duration_hold_code(self, test_job_queue_events):
        # The "46" _must_ be quoted, but that's wrong; it's not a string.
        assert SetAttribute("HoldReasonCode", "46") in test_job_queue_events


    # This test is kind of awful, but we don't have timestamps in the
    # job queue log, apparently.  Also, we don't actually record the
    # extents of the interval the shadow is actually checking explicitly,
    # but this should be close enough, given the slop involved.
    def test_allowed_job_duration_timing(self, test_job_log_events):
        for event in test_job_log_events:
            if event.type == htcondor.JobEventType.EXECUTE:
                test_job_started = event.timestamp
            if event.type == htcondor.JobEventType.JOB_HELD:
                test_job_held = event.timestamp

        test_job_duration = test_job_held - test_job_started
        assert test_job_duration <= ALLOWED_JOB_DURATION + PERIODIC_EXPR_INTERVAL

        # This asserts that the shadow took less than five second to activate
        # the claim.  If this proves unreliable, we can look into grovelling
        # the shadow log to determine the duration from claim activation to
        # the job going on hold, which should match ALLOWED_JOB_DURATION.
        assert test_job_duration > ALLOWED_JOB_DURATION - 5
