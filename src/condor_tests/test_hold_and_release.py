#!/usr/bin/env pytest

# this test replicates job_core_holdrelease_van

import logging


from ornithology import (
    config,
    standup,
    action,
    SetAttribute,
    SetJobStatus,
    JobStatus,
    in_order,
    SCRIPTS,
)


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@action
def job_queue_events_for_sleep_job(default_condor, path_to_sleep):
    handle = default_condor.submit(
        description={"executable": path_to_sleep, "arguments": "10",}, count=1,
    )
    jobid = handle.job_ids[0]

    default_condor.job_queue.wait_for_events(
        {
            jobid: [
                (  # when the job starts running, hold it
                    SetJobStatus(JobStatus.RUNNING),
                    lambda jobid, event: default_condor.run_command(
                        ["condor_hold", jobid]
                    ),
                ),
                (  # once the job is held, release it
                    SetJobStatus(JobStatus.HELD),
                    lambda jobid, event: default_condor.run_command(
                        ["condor_release", jobid]
                    ),
                ),
                SetJobStatus(JobStatus.COMPLETED),
            ]
        },
        timeout=600,
    )

    return default_condor.job_queue.by_jobid[jobid]


class TestCanHoldAndReleaseJob:
    def test_job_queue_events_in_correct_order(self, job_queue_events_for_sleep_job):
        assert in_order(
            job_queue_events_for_sleep_job,
            [
                SetJobStatus(JobStatus.IDLE),
                SetJobStatus(JobStatus.RUNNING),
                SetJobStatus(JobStatus.HELD),
                SetJobStatus(JobStatus.IDLE),
                SetJobStatus(JobStatus.RUNNING),
                SetJobStatus(JobStatus.COMPLETED),
            ],
        )

    def test_hold_reason_code_was_1(self, job_queue_events_for_sleep_job):
        assert SetAttribute("HoldReasonCode", "1") in job_queue_events_for_sleep_job
