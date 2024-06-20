#!/usr/bin/env pytest

import htcondor2 as htcondor

from ornithology import (
    standup,
    Condor,
    config,
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


@config(params={
    "vanilla": "vanilla",
    # Currently the same as vanilla, will become 'container' at some point.
    # "docker": "docker",
    "scheduler": "scheduler",
    "local": "local",
    # Can't be tested the usual personal condor.
    # "grid": "grid",
    }
)
def test_universe(request):
    return request.param

@action
def test_job_handle(condor, path_to_sleep, test_dir, test_universe):
    handle = condor.submit(
        description={
            "executable": path_to_sleep,
            "arguments": 2 * (ALLOWED_JOB_DURATION + PERIODIC_EXPR_INTERVAL + 1),
            "transfer_executable": False,
            "should_transfer_files": True,
            "universe": test_universe,
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
    # job queue log.  We also deliberately remove ShadowBday from the job
    # when it completes, so we can't check that, either.
    #
    # Instead, we assert that the job's binary was not running longer than
    # the longest possible time it could have been (assuming the shadow
    # claimed the startd in zero time, and that the periodic expression
    # interval was pessimally aligned), and that the job ran for strictly
    # more than one peridic expression interval.  We'd like a tighter lower
    # bound, but that test at least eliminates one class of errors.
    def test_allowed_job_duration_timing(self, test_job_log_events):
        for event in test_job_log_events:
            if event.type == htcondor.JobEventType.EXECUTE:
                test_job_started = event.timestamp
            if event.type == htcondor.JobEventType.JOB_HELD:
                test_job_held = event.timestamp

        test_job_duration = test_job_held - test_job_started
        # Allow two seconds of slop for rounding of seconds on each side
        assert test_job_duration <= ALLOWED_JOB_DURATION + PERIODIC_EXPR_INTERVAL + 2
        assert test_job_duration > PERIODIC_EXPR_INTERVAL
