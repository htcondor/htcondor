#!/usr/bin/env pytest


# Test that we can run a docker uni job

# Note this will probably not run in the regression test suite,
# so it isnt in the ctest lists, # but will run under a personal condor

import logging

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# Set up generic personal condor
@standup
def condor(test_dir):
    with Condor(test_dir / "condor") as condor:
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
