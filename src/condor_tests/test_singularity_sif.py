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
import subprocess
import time
from pathlib import Path
import shutil

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# Make a tiny sif file
def sif_file():
    os.mkdir("image_root")
    os.mkdir("image_root/etc")
    shutil.copyfile("/etc/passwd", "image_root/etc/passwd")

    # some singularities need mksquashfs in path
    # which some linuxes have in /usr/sbin
    os.environ["PATH"] = os.environ["PATH"] + ":/usr/sbin"

    for count in [0, 1, 2, 3, 4]:
        # rarely we see this failing in batlab with "bad file descriptor"
        # so, just retry.
        try:
            Path("empty.sif").unlink
        except FileNotFoundError:
            pass

        r = os.system("singularity -s build empty.sif image_root")
        if (r == 0):
            return "empty.sif"
        time.sleep(5)

    return False

@standup
def sif_file_fixture():
    return sif_file()

# Setup a personal condor 
@standup
def condor(test_dir):
    # Bind all the hosts's bin and lib dirs into the container, so can run /bin/cat and similar commands
    # without building a large container with lots of shared libraries
    with Condor(test_dir / "condor", config={"SINGULARITY_BIND_EXPR": "\"/bin:/bin /usr:/usr /lib:/lib /lib64:/lib64\""}) as condor:
        yield condor

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
        timeout=60,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )

    return ctj.query()[0]

# For the test to work, we need a singularity/apptainer which can work with
# SIF files, which is any version of apptainer, or singularity >= 3
def SingularityIsWorthy():
    result = subprocess.run("singularity --version", shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = result.stdout.decode('utf-8')

    logger.debug(output)
    if "apptainer" in output:
        return True

    if "3." in output:
        return True

    return False

def SingularityIsWorking():
    if not sif_file():
        return False

    result = subprocess.run("singularity exec -B/bin:/bin -B/lib:/lib -B/lib64:/lib64 -B/usr:/usr empty.sif /bin/ls /", shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = result.stdout.decode('utf-8')

    logger.debug(output)

    if result.returncode == 0:
        return True
    else:
        return False

# For the test to work, we need user namespaces to be working
# and enough of them.  This is a race, but better to try
# to test first.
def UserNamespacesFunctional():
    result = subprocess.run(["unshare", "-U", "/bin/sh", "-c", "exit 7"])
    if result.returncode == 7:
        print("unshare seems to work correctly, proceeding with test\n")
        return True
    else:
        print("unshare command failed, test cannot work, skipping test\n")
        return False

class TestContainerUni:
    @pytest.mark.skipif(not SingularityIsWorthy(), reason="No worthy Singularity/Apptainer found")
    @pytest.mark.skipif(not UserNamespacesFunctional(), reason="User namespaces not working -- some limit hit?")
    @pytest.mark.skipif(not SingularityIsWorking(), reason="Singularity doesn't seem to be working")
    def test_container_uni(self, sif_file_fixture, completed_test_job):
            assert completed_test_job['ExitCode'] == 0
