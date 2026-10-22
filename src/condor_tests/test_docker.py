#!/usr/bin/env pytest


# Test that we can run a docker uni job

# Note this will probably not run in the regression test suite,
# so it isnt in the ctest lists, # but will run under a personal condor

import logging
import re
import time

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

    def test_docker_image_hash(self, docker_job_ad, expected_image_hash):
        image_hash = docker_job_ad.get("DockerImageHash")
        assert image_hash is not None, "DockerImageHash missing from job ad"
        assert re.fullmatch(r"sha256:[0-9a-f]{64}", image_hash), \
            f"DockerImageHash '{image_hash}' is not a sha256 content hash"
        assert image_hash == expected_image_hash
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
