#!/usr/bin/env pytest

# this test replicates job_concurrency_limitsP

import logging

from ornithology import Condor, SetJobStatus, JobStatus, in_order, track_quantity

from conftest import config, standup, action

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# the effective number of slots should always be much larger than the number of
# jobs you plan to submit
SLOT_CONFIGS = {
    "static_slots": {"NUM_CPUS": "12", "NUM_SLOTS": "12"},
    "partitionable_slot": {
        "NUM_CPUS": "12",
        "SLOT_TYPE_1": "cpus=100%,memory=100%,disk=100%",
        "SLOT_TYPE_1_PARTITIONABLE": "True",
        "NUM_SLOTS_TYPE_1": "1",
    },
}


@config(params=SLOT_CONFIGS)
def slot_config(request):
    return request.param


CONCURRENCY_LIMITS = {
    "named_limit": {
        "config-key": "XSW_LIMIT",
        "config-value": "4",
        "submit-value": "XSW",
        "max-running": 4,
    },
    "default_limit": {
        "config-key": "CONCURRENCY_LIMIT_DEFAULT",
        "config-value": "2",
        "submit-value": "UNDEFINED:2",
        "max-running": 1,
    },
    "default_small": {
        "config-key": "CONCURRENCY_LIMIT_DEFAULT_SMALL",
        "config-value": "3",
        "submit-value": "small.license",
        "max-running": 3,
    },
    "default_large": {
        "config-key": "CONCURRENCY_LIMIT_DEFAULT_LARGE",
        "config-value": "1",
        "submit-value": "large.license",
        "max-running": 1,
    },
}


@standup
def condor(test_dir, slot_config):
    # set all of the concurrency limits for each slot config,
    # so that we can run all the actual job submits against the same config
    concurrency_limit_config = {
        v["config-key"]: v["config-value"] for v in CONCURRENCY_LIMITS.values()
    }

    with Condor(
        local_dir=test_dir / "condor",
        config={
            **slot_config,
            **concurrency_limit_config,
            # make the sure the negotiator runs many times within a single job duration
            "NEGOTIATOR_INTERVAL": "1",
        },
    ) as condor:
        yield condor


@action(params=CONCURRENCY_LIMITS)
def concurrency_limit(request):
    return request.param


@action
def handle(condor, concurrency_limit):
    handle = condor.submit(
        description={
            "executable": "/bin/sleep",
            "arguments": "5",
            "request_memory": "100MB",
            "request_disk": "10MB",
            "concurrency_limits": concurrency_limit["submit-value"],
        },
        count=(concurrency_limit["max-running"] + 1) * 2,
    )

    condor.job_queue.wait_for_events(
        {
            jobid: [
                (
                    SetJobStatus(JobStatus.RUNNING),
                    lambda j, e: condor.run_command(["condor_q"], echo=True),
                ),
                SetJobStatus(JobStatus.COMPLETED),
            ]
            for jobid in handle.job_ids
        },
        timeout=60,
    )

    yield handle

    handle.remove()


@action
def num_jobs_running_history(condor, handle, concurrency_limit):
    return track_quantity(
        condor.job_queue.filter(lambda j, e: j in handle.job_ids),
        increment_condition=lambda id_event: id_event[-1]
        == SetJobStatus(JobStatus.RUNNING),
        decrement_condition=lambda id_event: id_event[-1]
        == SetJobStatus(JobStatus.COMPLETED),
        max_quantity=concurrency_limit["max-running"],
        expected_quantity=concurrency_limit["max-running"],
    )


@action
def startd_log_file(condor):
    return condor.startd_log.open()


@action
def num_busy_slots_history(startd_log_file, handle, concurrency_limit):
    logger.debug("Checking Startd log file...")
    logger.debug("Expected Job IDs are: {}".format(handle.job_ids))

    active_claims_history = track_quantity(
        startd_log_file.read(),
        increment_condition=lambda msg: "Changing activity: Idle -> Busy" in msg,
        decrement_condition=lambda msg: "Changing activity: Busy -> Idle" in msg,
        max_quantity=concurrency_limit["max-running"],
        expected_quantity=concurrency_limit["max-running"],
    )

    return active_claims_history


class TestConcurrencyLimits:
    def test_all_jobs_ran(self, condor, handle):
        for jobid in handle.job_ids:
            assert in_order(
                condor.job_queue.by_jobid[jobid],
                [
                    SetJobStatus(JobStatus.IDLE),
                    SetJobStatus(JobStatus.RUNNING),
                    SetJobStatus(JobStatus.COMPLETED),
                ],
            )

    def test_never_more_jobs_running_than_limit(
        self, num_jobs_running_history, concurrency_limit
    ):
        assert max(num_jobs_running_history) <= concurrency_limit["max-running"]

    def test_num_jobs_running_hits_limit(
        self, num_jobs_running_history, concurrency_limit
    ):
        assert concurrency_limit["max-running"] in num_jobs_running_history

    def test_never_more_busy_slots_than_limit(
        self, num_busy_slots_history, concurrency_limit
    ):
        assert max(num_busy_slots_history) <= concurrency_limit["max-running"]

    def test_num_busy_slots_hits_limit(self, num_busy_slots_history, concurrency_limit):
        assert concurrency_limit["max-running"] in num_busy_slots_history
