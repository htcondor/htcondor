#!/usr/bin/env pytest

# Regression test for condor_ssh_to_job against a job whose scratch dir is
# an LVM-backed mount: the sshd forks from the same (post-unshare) starter
# process as the job, so an ssh-to-job session should land in the same
# mounted LV. See src/condor_starter.V6.1/sshd_proc.cpp and
# condor_ssh_to_job_shell_setup.

import logging
import time

import pytest

from ornithology import *

from liblvm import LVMTestable, LVM_SKIP_REASON, lvm_config

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

pytestmark = pytest.mark.skipif(not LVMTestable(), reason=LVM_SKIP_REASON)

SCRATCH_PATH_POLL_TIMEOUT = 120


@config(params={"thin": True, "thick": False})
def thin_provisioning(request):
    return request.param


@standup
def condor(test_dir, thin_provisioning):
    with Condor(
        test_dir / "condor",
        config=lvm_config(thin_provisioning, test_dir / "condor" / "lvm_backing.img"),
    ) as condor:
        yield condor


@action
def scratch_path_file(test_dir):
    return test_dir / "scratch_path.txt"


@action
def ssh_test_job_hash(scratch_path_file):
    return {
        "shell": f"echo $_CONDOR_SCRATCH_DIR > {scratch_path_file}; sleep 150",
        "universe": "vanilla",
        "output": "output",
        "error": "error",
        "log": "ssh_test_log",
        "request_cpus": "1",
        "request_memory": "64m",
        "request_disk": "48m",
    }


@action
def running_ssh_test_job(condor, ssh_test_job_hash):
    job = condor.submit({**ssh_test_job_hash}, count=1)
    assert job.wait(
        condition=ClusterState.any_running,
        timeout=120,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    yield job
    # Wait for removal to finish before pool teardown, or condor_off has to
    # wait out its own drain/kill_after timeout for a mid-teardown claim.
    job.remove()
    job.wait(condition=ClusterState.all_terminal, timeout=120, verbose=True)


def _wait_for_scratch_path(scratch_path_file):
    deadline = time.time() + SCRATCH_PATH_POLL_TIMEOUT
    while time.time() < deadline:
        if scratch_path_file.exists():
            contents = scratch_path_file.read_text().strip()
            if contents:
                return contents
        time.sleep(1)
    return None


def _parse_kv_lines(output):
    info = {}
    for line in output.splitlines():
        key, _, value = line.partition("=")
        if _:
            info[key] = value
    return info


class TestLVMSshToJob:
    def test_ssh_session_shares_job_mount(
        self, condor, running_ssh_test_job, scratch_path_file
    ):
        job_scratch_dir = _wait_for_scratch_path(scratch_path_file)
        assert job_scratch_dir, "job never reported its _CONDOR_SCRATCH_DIR"

        # The shell_setup script's cd into _CONDOR_SCRATCH_DIR only runs for
        # interactive sessions, so cd explicitly for this one-shot command.
        remote_cmd = (
            "cd \"$_CONDOR_SCRATCH_DIR\" && "
            "echo SCRATCH=$_CONDOR_SCRATCH_DIR; "
            "echo PWD=$(pwd); "
            "touch ssh_marker && stat -c DEV=%d ."
        )
        result = condor.run_command(
            [
                "condor_ssh_to_job",
                "-auto-retry",
                str(running_ssh_test_job.clusterid),
                remote_cmd,
            ],
            timeout=120,
        )

        assert result.returncode == 0, result.stderr

        info = _parse_kv_lines(result.stdout)
        assert info.get("SCRATCH") == job_scratch_dir
        assert info.get("PWD") == job_scratch_dir
        assert "DEV" in info
