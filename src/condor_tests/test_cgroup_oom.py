#!/usr/bin/env pytest

# Test that we can create sub-cgroups.  If not, skip the test
# if so, turn on the secret unprivileged cgroup option,
# and submit a job that uses more memory than we have
# requested.  Verify that it goes on hold.

import logging
import pytest
import os

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
            "log": "log",
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
            "log": "log",
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


# This test only works if we start in a writeable (delegated)
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


class TestCgroupOOM:
    @pytest.mark.skipif(not CgroupIsWriteable(), reason="Test was not born in a writeable cgroup v2")
    def test_cgroup_oom(self, completed_test_job):
        assert completed_test_job.state.all_held()

    @pytest.mark.skipif(not CgroupIsWriteable(), reason="Test was not born in a writeable cgroup v2")
    def test_cgroup_escape(self, escaping_test_job):
        with open('pidfile', 'r') as file:
            pid = file.read().rstrip()
        assert not os.path.exists("/proc/" + pid)
