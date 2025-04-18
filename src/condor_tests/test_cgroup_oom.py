#!/usr/bin/env pytest

# Test that we can create sub-cgroups.  If not, skip the test
# if so, turn on the secret unprivileged cgroup option,
# and submit a job that uses more memory than we have
# requested.  Verify that it goes on hold.

import logging
import pytest
import os

import htcondor as htcondor2
from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# Standup a condor with user-space cgroup v2 enabled
@standup
def condor(test_dir):
    with Condor(test_dir / "condor", config={"CREATE_CGROUP_WITHOUT_ROOT": "true"}) as condor:
        yield condor

# The actual job will allocate 200 Mb in python, but request 50Mb.
# The 50Mb might get rounded up to 128, so be careful.
@action
def job_script_contents():
    return format_script( """
#!/usr/bin/env python3

import os
import time

two_hundred_meg = bytearray(200 * 1024 * 1024)
for i in range(30):
    time.sleep(1)
exit(0)
""")

@action
def test_script(test_dir, job_script_contents):
    test_script = test_dir / "job_script.py"
    write_file(test_script, job_script_contents)
    return test_script


@action
def test_job_hash(test_dir, path_to_python, test_script):
    return {
            "executable": path_to_python,
            "arguments": test_script,
            "universe": "vanilla",
            "output": "output",
            "error": "error",
            "log": "oom_log",
            "keep_claim_idle": "300",
            "request_memory": "50m",
            "request_cpus": "1",
            "request_disk": "200m"
            }

@action
def completed_test_job(condor, test_job_hash):
    ctj = condor.submit(
        {**test_job_hash}, count=1
    )
    assert ctj.wait(
        condition=ClusterState.all_held,
        timeout=120,
        verbose=True,
        fail_condition=ClusterState.any_complete,
    )
    return ctj

@action
def escaping_job_hash(test_dir, path_to_python, test_script):
    # A job which spawns "sleep 1234", with all the condor tracking env vars removed
    # and nohup'd, to avoid all non-cgroup tracking mechanisms.  Writes the pid
    # to pid file, which lives in the test_dir.  If the pid in pidfile exists
    # after the job exits, the sleep escaped the job, and the test should fail.
    return {
            "shell": "nohup /usr/bin/env -i /usr/bin/sleep 1234 & echo $! > pidfile",
            "universe": "vanilla",
            "output": "output",
            "error": "error",
            "log": "escaped_log",
            "keep_claim_idle": "300",
            "request_memory": "50m",
            "request_cpus": "1",
            "request_disk": "200m"
            }

@action
def escaping_test_job(condor, escaping_job_hash):
    ej = condor.submit(
        {**escaping_job_hash}, count=1
    )
    assert ej.wait(
        condition=ClusterState.all_complete,
        timeout=120,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    return ej

@action
def isolated_job_hash(test_dir, path_to_python):
    # A job which cats the content of its own cgroup.procs.  Because we
    # exec, the shell has gone away, so the cgroup should only
    # contain the cat process, which verifies that the job is isolated into
    # its own cgroup.
    return {
            "shell": "exec /usr/bin/cat /sys/fs/cgroup$(awk -F: '/^0:/ {print $3}' < /proc/self/cgroup)/cgroup.procs > cgroup.procs",
            "universe": "vanilla",
            "output": "output",
            "error": "error",
            "log": "isolated_log",
            "keep_claim_idle": "300",
            "request_memory": "50m",
            "request_cpus": "1",
            "request_disk": "200m"
            }


@action
def isolated_test_job(condor, isolated_job_hash):
    ij = condor.submit(
        {**isolated_job_hash}, count=1
    )
    assert ij.wait(
        condition=ClusterState.all_complete,
        timeout=120,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    return ij

@action
def slice_scope_job_hash(test_dir, path_to_python):
    # A job which cats the content of /proc/self/cgroup.
    # This can be interrogated to verify that the job is the right cgroup.  
    return {
            "shell": "/usr/bin/cat /proc/self/cgroup > cgroup",
            "universe": "vanilla",
            "output": "output",
            "error": "error",
            "log": "slice_scope_log",
            "keep_claim_idle": "300",
            "request_memory": "50m",
            "request_cpus": "1",
            "request_disk": "200m"
            }

@action
def slice_scope_test_job(condor, slice_scope_job_hash):
    ssj = condor.submit(
        {**slice_scope_job_hash}, count=1
    )
    assert ssj.wait(
        condition=ClusterState.all_complete,
        timeout=120,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    return ssj

@action
def memory_detection_job_hash(test_dir, path_to_python):
    # A job which runs condor_config_val DETECTED_MEMORY.
    # This should return the memory limit of the cgroup
    bin_dir = htcondor2.param["BIN"]
    return {
            "shell": f"{bin_dir}/condor_config_val DETECTED_MEMORY",
            "environment": "CONDOR_CONFIG=/dev/null",
            "universe": "vanilla",
            "output": "detected_memory",
            "error": "error",
            "log": "detected_memory.log",
            "keep_claim_idle": "300",
            "request_memory": "50m",
            "request_cpus": "1",
            "request_disk": "200m"
            }

@action
def memory_detection_test_job(condor, memory_detection_job_hash):
    mmj = condor.submit(
        {**memory_detection_job_hash}, count=1
    )
    assert mmj.wait(
        condition=ClusterState.all_complete,
        timeout=120,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    return mmj

# These tests only works if we start in a writeable (delegated)
# cgroup.  Return true if this is so
def CgroupIsWriteable():
    with open('/proc/self/cgroup') as fd:
        proc_self_cgroup = fd.read()

    # proc_self_cgroup on a cgroup v2 system should contain
    # ::/path/to/cgroup/without/leading/sys/fs/cgroup
    my_cgroup = proc_self_cgroup.split(':')[2].rstrip()
    cgroup_filepath = "/sys/fs/cgroup" + my_cgroup
    cgroup_filepath_parent = cgroup_filepath + "/.."

    # technically, also need to verify that parent's cgroup.subtree_control
    # contains memory, pid, cpu, but let's skip that for now
    return os.access(cgroup_filepath_parent, os.W_OK)


@pytest.mark.skipif(not CgroupIsWriteable(), reason="Test was not born in a writeable cgroup v2")
class TestCgroupOOM:
    def test_cgroup_oom(self, completed_test_job):
        assert completed_test_job.state.all_held()

    def test_cgroup_escape(self, escaping_test_job):
        # When the job exits, pidfile contains the pid of the sleep job
        with open('pidfile', 'r') as file:
            pid = file.read().rstrip()
        # Check that the pid does not exist in the system anymore -- that it has not escaped.
        assert not os.path.exists("/proc/" + pid)

    def test_cgroup_isolated(self, isolated_test_job):
        # When the job exits, this is a copy of the cgroup.procs file
        # from the cgroup of the job
        cgroup_procs = "cgroup.procs"
        assert os.path.exists(cgroup_procs)
        with open(cgroup_procs, 'r') as file:
            for count, pid in enumerate(file):
                pass
        count += 1 # Python has 0-indexing
        assert count == 1

    def test_cgroup_slice_scope(self, slice_scope_test_job):
        # When the job exits, cgroup contains the cgroup of the cat job
        with open('cgroup', 'r') as file:
            line        = file.read().rstrip()
            ultimate    = line.split('/')[-1]
            penultimate = line.split('/')[-2]
            assert ultimate.endswith(".scope")
            assert penultimate.endswith(".slice")
        assert True

    def test_cgroup_memory_detection(self, memory_detection_test_job):
        # When the job exits, detected_memory contains condor's idea of memory
        memory = 0
        with open('detected_memory', 'r') as file:
            memory = int(file.read().rstrip())
        assert(memory < 200 and memory > 50) # Allow for rounding up to 128 Mb
