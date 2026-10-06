#!/usr/bin/env pytest


# Test that we can run a docker uni job

# Note this will probably not run in the regression test suite,
# so it isnt in the ctest lists, # but will run under a personal condor

import logging

from ornithology import *
from htcondor2 import JobEventType

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# Set up generic personal condor
@standup
def condor(test_dir):
    with Condor(test_dir / "condor") as condor:
        yield condor

# A personal condor where every docker create fails, but the image pull
# (which happens before create) succeeds
@standup
def condor_bad_create(test_dir):
    with Condor(
        test_dir / "condor_bad_create",
        config={"DOCKER_EXTRA_ARGUMENTS": "--htcondor-test-bogus-flag"},
    ) as condor:
        yield condor

@action
def test_job_hash(test_dir, path_to_python):
    return {
            "executable": "/bin/ps",
            "arguments": "auxww",
            "universe": "docker",
            "docker_image": "busybox",
            "output": "output",
            "error": "error",
            "log": "log",
            "should_transfer_files": "yes",
            "when_to_transfer_output": "on_exit",
            }

@action
def completed_test_job(condor, test_job_hash):
    ctj = condor.submit(
        {**test_job_hash}, count=1
    )

    job_id = JobID(ctj.clusterid, 0)

    assert condor.job_queue.wait_for_events(
            expected_events={job_id: [SetJobStatus(JobStatus.COMPLETED)]},
            unexpected_events={job_id: [SetJobStatus(JobStatus.HELD)]},
            timeout=60
            )
    return ctj

@action
def events_for_docker_job(condor, completed_test_job):
    return condor.job_queue.by_jobid[JobID(completed_test_job.clusterid,0)]

@action
def bad_create_job(condor_bad_create, test_job_hash):
    job = condor_bad_create.submit({**test_job_hash}, count=1)
    assert job.wait(condition=ClusterState.all_held, timeout=120)
    return job

def event_types(handle):
    # read_events() only yields unread events; .events holds all read so far
    list(handle.event_log.read_events())
    return [e.type for e in handle.event_log.events]

class TestDocker:
    def test_docker(self, events_for_docker_job):
        assert in_order(
                events_for_docker_job,
                [
                    SetJobStatus(JobStatus.IDLE),
                    SetJobStatus(JobStatus.RUNNING),
                    SetJobStatus(JobStatus.COMPLETED),
                ],
             )

    def test_execute_event_before_terminate(self, completed_test_job):
        types = event_types(completed_test_job)
        assert JobEventType.EXECUTE in types
        assert types.index(JobEventType.EXECUTE) < types.index(JobEventType.JOB_TERMINATED)

    # The execute event should be logged when the container is started,
    # not when it is created, so a failed docker create logs no execute event
    def test_no_execute_event_on_create_failure(self, bad_create_job):
        types = event_types(bad_create_job)
        assert JobEventType.JOB_HELD in types
        assert JobEventType.EXECUTE not in types
