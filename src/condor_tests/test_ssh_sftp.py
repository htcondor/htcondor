#!/usr/bin/env pytest

#
# This test checks condor_ssh_to_job -sftp
# We launch a sleep job, sftp a file to the job
# then from the job.

import htcondor

from ornithology import (
    config,
    standup,
    action,
    JobStatus,
    ClusterState,
    Condor,
    logging,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

@standup
def condor(test_dir):
    with Condor(test_dir / "condor") as condor:
        yield condor


@action
def sleep_job_parameters(path_to_sleep):
    return {
        "executable": path_to_sleep,
        "transfer_executable": "true",
        "should_transfer_files": "true",
        "universe": "vanilla",
        "arguments": "600",
        "request_cpus": "1",
        "request_disk": "100",
        "request_memory": "1024",
        "log": "sleep.log",
    }

@action
def sleep_job(sleep_job_parameters, condor):
    job = condor.submit( sleep_job_parameters)
    assert job.wait(
        condition=ClusterState.all_running,
        timeout=600,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    return job

@action
def ssh_ls(condor, sleep_job):
    cp = condor.run_command(['condor_ssh_to_job', '-auto-retry', '1.0', '/bin/ls'])
    assert cp.returncode == 0
    return True

@action
def sftp_ssh_ls(condor, sleep_job):
    cp = condor.run_command(['/bin/echo', 'ls', '|', 'condor_ssh_to_job', '-ssh', 'sftp', '1.0'])
    assert cp.returncode == 0
    return True

@action
def local_file():
    with open("local_file", "w") as f:
        f.write("This is a local file\n")

@action
def scp_ssh_put(condor, sleep_job, local_file):
    cp = condor.run_command(['condor_ssh_to_job', '-ssh', 'scp', '1.0', 'local_file', 'remote:sandbox_copy'])
    assert cp.returncode == 0
    return True

@action
def scp_ssh_get(condor, sleep_job):
    cp = condor.run_command(['condor_ssh_to_job', '-ssh', 'scp', '1.0', 'remote:sandbox_copy', 'local_copy'])
    assert cp.returncode == 0
    return True

class TestSshSftp:
    def test_ssh(self, ssh_ls):
        assert True 
    def test_sftp_put(self, scp_ssh_put):
        assert True 
    def test_scp_put(self, scp_ssh_put):
        assert True 
    def test_scp_get(self, scp_ssh_get):
        assert True 


