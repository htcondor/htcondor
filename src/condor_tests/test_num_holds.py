#!/usr/bin/env pytest

from ornithology import (
    config,
    standup,
    action,
    Condor,
    SetAttribute,
    SetJobStatus,
    JobStatus,
    in_order,
)

@standup
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor", 
        config={
            "DAEMON_LIST": "SCHEDD MASTER",
            "USE_SHARED_PORT": False
        }
    ) as condor:
        yield condor

@action
def job_queue_events_for_sleep_job(condor, path_to_sleep):
    # Submit a single job that sleeps for 0 seconds.
    handle = condor.submit(
            description={"executable": path_to_sleep, "arguments": "1", 
                "universe": "local",
                "on_exit_hold": "true"
                },
        count=1,
    )

    # The id (cluster.proc) of the first (and only) job in the submit.
    jobid = handle.job_ids[0]

    # Wait for (and react to) events in the job queue log.
    condor.job_queue.wait_for_events(
        {  
            jobid: [
                # wait for the job to go on hold
                SetJobStatus(JobStatus.HELD),
            ]
        },
        # All waits must have a timeout.
        timeout=120,
    )

    # This returns all of the job events in the job queue log for the jobid,
    # making that information available to downstream fixtures/tests.
    return condor.job_queue.by_jobid[jobid]


# All tests must be methods of classes; the name of the class must be Test*
class TestJobHoldAggregates:
    # Methods that begin with test_* are tests.

    # Test to check that NumReasonCode=3 (JobPolicy)
    def test_hold_reason_code_was_3(self, job_queue_events_for_sleep_job):
        assert SetAttribute("HoldReasonCode", "3") in job_queue_events_for_sleep_job
    
    # Test to check that NumHoldsByReason = [ JobPolicy = 1 ]
    def test_hold_num_holds_was_1(self, job_queue_events_for_sleep_job):
        assert SetAttribute("NumHoldsByReason", "[ JobPolicy = 1 ]") in job_queue_events_for_sleep_job

    # Test to check that NumHolds = 1
    def test_hold_num_holds_was_1(self, job_queue_events_for_sleep_job):
        assert SetAttribute("NumHolds", "1") in job_queue_events_for_sleep_job
