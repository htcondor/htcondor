#!/usr/bin/env pytest


# Test that we can run a docker uni job

# Note this will probably not run in the regression test suite,
# so it isnt in the ctest lists, # but will run under a personal condor

import logging
import re
import time

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

@action
def docker_job_ad(condor, completed_test_job):
    # The job has completed, but it may not have reached the history file yet.
    schedd = condor.get_local_schedd()
    constraint = f"ClusterId == {completed_test_job.clusterid} && ProcId == 0"
    deadline = time.time() + 60
    while time.time() < deadline:
        ads = list(schedd.history(constraint, ["DockerImage", "DockerImageHash"]))
        if len(ads) > 0:
            return ads[0]
        time.sleep(1)
    assert False, f"Job {completed_test_job.clusterid}.0 never reached the history file"

@action
def expected_image_hash(condor, test_job_hash, completed_test_job):
    # After the job has run (so the image is pulled), ask docker directly
    # which image the tag resolves to, using the same docker binary the
    # starter uses.
    docker = condor.run_command(["condor_config_val", "DOCKER"]).stdout.strip()
    rv = condor.run_command(
        [docker, "image", "inspect", "--format", "{{.Id}}", test_job_hash["docker_image"]]
    )
    assert rv.returncode == 0, f"docker image inspect failed: {rv.stderr}"
    return rv.stdout.strip()

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

    def test_docker_image_hash(self, docker_job_ad, expected_image_hash):
        image_hash = docker_job_ad.get("DockerImageHash")
        assert image_hash is not None, "DockerImageHash missing from job ad"
        assert re.fullmatch(r"sha256:[0-9a-f]{64}", image_hash), \
            f"DockerImageHash '{image_hash}' is not a sha256 content hash"
        assert image_hash == expected_image_hash
