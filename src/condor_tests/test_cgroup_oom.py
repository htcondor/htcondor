#!/usr/bin/env pytest


# Test that we can set environment variables in the submit
# file, and they make it to the job, along with the magic
# $$(CondorScratchDir) token expanded properly

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

# The actual job will just print environment vars of interest to stdout
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
        assert True
