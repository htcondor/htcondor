#!/usr/bin/env pytest


# Test that we can run simple container universe
# jobs with "singularity", by configuring condor
# to mock the singularity executable with a simple
# wrapper script.

import logging
import htcondor

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# Setup a personal condor with SINGULARITY pointing at the mocking wrapper
@standup
def condor(test_dir):
    # Singularity wrapper is in the root of the per-test directory, two
    # levels above the condor config
    path_to_singularity = test_dir / "../../singularity_tester.sh"
    with Condor(test_dir / "condor", config={"SINGULARITY": path_to_singularity}) as condor:
        yield condor

# The actual job will cat a file
@action
def test_job_hash(test_dir):
    return {
            "universe": "container",
            "container_image": "ignored.sif",
            "executable": "/bin/cat",
            "arguments": "/etc/hosts",
            "output": "output",
            "error": "error",
            "log": "log",
            }

@action
def completed_test_job(condor, test_job_hash):
    ctj = condor.submit(
        {**test_job_hash}, count=1
    )
    assert ctj.wait(
        condition=ClusterState.all_terminal,
        timeout=60,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    return ctj

class TestContainerUni:
    def test_container_uni(self, completed_test_job):
            assert True
