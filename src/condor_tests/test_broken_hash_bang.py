#!/usr/bin/env pytest


# Test that when we submit a job with a broken #! interpreter
# line we get some kind of appropriate hold message

import logging
import htcondor
import os

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# Set a job environment variable in the config file
@standup
def condor(test_dir):
    with Condor(test_dir / "condor") as condor:
        yield condor

# The actual job with a broken #! line
@action
def test_script_contents():
    return format_script("""
#!/bin/sh.not 

exit 0
""")

@action
def test_script(test_dir, test_script_contents):
    test_script = test_dir / "test_script.sh"
    write_file(test_script, test_script_contents)
    return test_script


@action
def test_job_hash(test_dir, test_script):
    return {
            "executable": test_script,
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
            condition=ClusterState.all_held,
            timeout=60,
            verbose=True,
            fail_condition=ClusterState.any_complete,
            )
    return ctj

class TestBrokenHashBang:
    def test_broken_hash_bang(self, completed_test_job):

        with open("log","r") as f:
            assert 0 != len([i for i in f.readlines() if 'invalid interpreter' in i])

