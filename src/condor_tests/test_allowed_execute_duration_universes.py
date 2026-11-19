#!/usr/bin/env pytest

#
# Verify that allowed_execute_duration puts a job on hold in the
# universes that don't run under a shadow (scheduler and local), as
# well as vanilla for comparison.  The schedd itself must record
# JobCurrentStartExecutingDate for the shadow-less universes.
#

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

ALLOWED_EXECUTE_DURATION = 20
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
    "scheduler": "scheduler",
    "local": "local",
    }
)
def test_universe(request):
    return request.param


@action
def test_job_handle(condor, path_to_sleep, test_dir, test_universe):
    handle = condor.submit(
        description={
            "executable": path_to_sleep,
            "arguments": 2 * (ALLOWED_EXECUTE_DURATION + PERIODIC_EXPR_INTERVAL + 1),
            "transfer_executable": False,
            "should_transfer_files": True,
            "universe": test_universe,
            "log": test_dir / "test_job.log",
            "on_exit_remove": False,
            "allowed_execute_duration": ALLOWED_EXECUTE_DURATION,
        },
        count=1
    )

    job_id = handle.job_ids[0]
    timeout = 4 * (ALLOWED_EXECUTE_DURATION + PERIODIC_EXPR_INTERVAL + 1)

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


class TestAllowedExecuteDurationUniverses:
    def test_allowed_execute_duration_sequence(self, test_job_queue_events):
        assert in_order(
            test_job_queue_events,
            [
                SetJobStatus(JobStatus.IDLE),
                SetJobStatus(JobStatus.RUNNING),
                SetJobStatus(JobStatus.HELD),
            ]
        )


    def test_allowed_execute_duration_hold_code(self, test_job_queue_events):
        # 47 is CONDOR_HOLD_CODE::JobExecuteExceeded.  As in
        # test_allowed_job_duration, the value must be quoted.
        assert SetAttribute("HoldReasonCode", "47") in test_job_queue_events


    # See test_allowed_job_duration for why this timing check is loose:
    # the job ran no longer than the longest possible time before the
    # periodic policy could have fired, and strictly longer than one
    # periodic expression interval.
    def test_allowed_execute_duration_timing(self, test_job_log_events):
        for event in test_job_log_events:
            if event.type == htcondor.JobEventType.EXECUTE:
                test_job_started = event.timestamp
            if event.type == htcondor.JobEventType.JOB_HELD:
                test_job_held = event.timestamp

        test_job_duration = test_job_held - test_job_started
        # Allow two seconds of slop for rounding of seconds on each side
        assert test_job_duration <= ALLOWED_EXECUTE_DURATION + PERIODIC_EXPR_INTERVAL + 2
        assert test_job_duration > PERIODIC_EXPR_INTERVAL
