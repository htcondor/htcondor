#!/usr/bin/env pytest

# Make sure DAGMan utils doesn't crash when non-newline terminated
# line filled with whitespace exists (common w/ python file writes)
# See: https://opensciencegrid.atlassian.net/browse/HTCONDOR-2463

from ornithology import *
import htcondor

@action
def submit_from_dag():
    filename = "test.dag"
    with open(filename, "w") as f:
        f.write("""
                JOB A sample
                """)
    return htcondor.Submit.from_dag(filename, {"force" : True})

class TestDAGManUtilsCrash:
    def test_dagman_utils_crash(self, submit_from_dag):
        assert submit_from_dag != None