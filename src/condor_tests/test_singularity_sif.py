#!/usr/bin/env pytest


# Test that we can run simple container universe
# jobs with a real singularity, assuming it
# is installed and works

# Test plan is
# If we can run singularity build on an empty directory and get
# a sif file without an error, assume that we can run singularity
# jobs.  If not, skip the test.

# Then, stand up a condor with SINGULARITY_BIND_EXPR set to all
# the /bin, /usr/bin and /lib directories from the host, so that
# we can run any binary from the host inside the container without
# worrying about also copying in shared libraries, helper files,
# config files etc.

# The first test is to just cat /singularity, which is a file which 
# will only exist inside the container. If that exits zero,
# assume success.

import logging
import os,sys
import pytest
from pathlib import Path

from ornithology import *

from libcontainer import (
    SingularityIsWorthy,
    UserNamespacesFunctional,
    SingularityIsWorking,
    make_empty_sif,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

@standup
def sif_file_fixture():
    return make_empty_sif("empty.sif")

# Setup a personal condor 
@standup
def condor(test_dir):
    # Bind all the hosts's bin and lib dirs into the container, so can run /bin/cat and similar commands
    # without building a large container with lots of shared libraries
    with Condor(test_dir / "condor", config={
        "SINGULARITY_BIND_EXPR": "\"/bin:/bin /usr:/usr /lib:/lib /lib64:/lib64\"",
        "SINGULARITY_TEST_SANDBOX_TIMEOUT":              "50",
        "SINGULARITY": "/usr/bin/singularity"
        }) as condor:
        yield condor

# Setup a personal condor with SINGULARITY_TARGET_DIR set
@standup
def condor_with_td(test_dir):
    with Condor(test_dir / "condor_td", config={
        "SINGULARITY_BIND_EXPR": "\"/bin:/bin /usr:/usr /lib:/lib /lib64:/lib64\"",
        "SINGULARITY":           "/usr/bin/singularity",
        "SINGULARITY_TARGET_DIR": "/td"}) as condor_with_td:
        yield condor_with_td

# The actual job will cat a file
# The actual job will cat a file
@action
def test_job_hash():
    return {
            "universe": "container",
            "container_image": "empty.sif",
            "executable": "/bin/cat",
            "arguments": "/singularity",
            "should_transfer_files": "yes",
            "when_to_transfer_output": "on_exit",
            "output": "output",
            "error": "error",
            "log": "log",
            "leave_in_queue": "True"
            }

@action
def completed_test_job(condor, test_job_hash):

    ctj = condor.submit(
        {**test_job_hash}, count=1
    )

    assert ctj.wait(
        condition=ClusterState.all_terminal,
        timeout=300,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )

    return ctj.query()[0]

@action
def test_job_hash_with_xfer():
    return {
            "universe": "container",
            "container_image": "empty.sif",
            "executable": "/bin/cp",
            "arguments": "an_input_file an_output_file",
            "transfer_input_files": "an_input_file",
            "should_transfer_files": "yes",
            "when_to_transfer_output": "on_exit",
            "transfer_executable": "False",
            "output": "output",
            "error": "error",
            "log": "log_td",
            "leave_in_queue": "True"
            }
# Test that file xfer works with SINGULARITY_TARGET_DIR
@action
def completed_test_job_with_xfer(condor_with_td, test_job_hash_with_xfer):

    with open("an_input_file", "w") as f:
        f.write("This is the input file\n")

    ctj_td = condor_with_td.submit(
        {**test_job_hash_with_xfer}, count=1
    )

    assert ctj_td.wait(
        condition=ClusterState.all_terminal,
        timeout=300,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    return ctj_td.query()[0]


@action
def ssh_job_hash():
    return {
            "universe": "container",
            "container_image": "empty.sif",
            "executable": "/bin/sleep",
            "arguments": "600",
            "should_transfer_files": "yes",
            "when_to_transfer_output": "on_exit",
            "output": "ssh_output",
            "error": "ssh_error",
            "log": "ssh_log",
            }

@action
def running_ssh_job(condor, ssh_job_hash):
    job = condor.submit(ssh_job_hash, count=1)
    assert job.wait(
        condition=ClusterState.all_running,
        timeout=300,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    return job

@action
def ssh_to_container_job(condor, running_ssh_job):
    cp = condor.run_command(['condor_ssh_to_job', '-auto-retry', f'{running_ssh_job.clusterid}.0', '/bin/echo', 'hello_from_container'])
    return cp

@pytest.mark.skipif(not SingularityIsWorthy(), reason="No worthy Singularity/Apptainer found")
@pytest.mark.skipif(not UserNamespacesFunctional(), reason="User namespaces not working -- some limit hit?")
@pytest.mark.skipif(not SingularityIsWorking(), reason="Singularity doesn't seem to be working")
class TestSingularitySIF:
    def test_container_uni(self, sif_file_fixture, completed_test_job):
            assert completed_test_job['ExitCode'] == 0
    def test_container_uni_with_xfer(self, completed_test_job_with_xfer):
            assert completed_test_job_with_xfer['ExitCode'] == 0
    def test_ssh_to_container_job(self, sif_file_fixture, ssh_to_container_job):
            assert ssh_to_container_job.returncode == 0
            assert "hello_from_container" in ssh_to_container_job.stdout
