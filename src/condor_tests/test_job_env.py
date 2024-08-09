#!/usr/bin/env pytest


# Test that we can set environment variables in the submit
# file, and they make it to the job, along with the magic
# $$(CondorScratchDir) token expanded properly

import logging

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# Set a job environment variable in the config file
@standup
def condor(test_dir):
    with Condor(test_dir / "condor", config={"STARTER_JOB_ENVIRONMENT": "\"SAMPLE_VAR=AnExample\""}) as condor:
        yield condor

# The actual job will just print environment vars of interest to stdout
@action
def test_script_contents():
    return format_script( """
#!/usr/bin/env python3

import os

print(os.environ["VAR_FROM_SUBMIT"])
print(os.environ["_CONDOR_SCRATCH_DIR"] + "/foo")
print(os.environ["SAMPLE_VAR"])
exit(0)
""")

@action
def test_script(test_dir, test_script_contents):
    test_script = test_dir / "test_script.py"
    write_file(test_script, test_script_contents)
    return test_script


@action
def test_job_hash(test_dir, path_to_python, test_script):
    return {
            "executable": path_to_python,
            "arguments": test_script,
            "environment": "VAR_FROM_SUBMIT=$$(CondorScratchDir)/foo",
            "universe": "vanilla",
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

class TestJobEnv:
    def test_job_env(self, completed_test_job):

        with open("output","r") as f:
            # First line is the env from the job submit file "environment = VAR_FROM_SUBMIT=$$(CondorScratchDir)"
            scratchDirFromJob = f.readline()
            # Next line is the expanded $_CONDOR_SCRATCH_DIR from the running job + /foo, should be same as above
            scratchDirFromEnv = f.readline()
            assert scratchDirFromEnv == scratchDirFromJob
            
            # Next line is the SAMPLE_VAR from STARTER_JOB_ENVIRONMENT
            sampleVar = f.readline()
            assert sampleVar == "AnExample\n"
