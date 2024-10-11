#!/usr/bin/env pytest

#   test_dagman_service_nodes.py
#
#   This test makes sure that service nodes dont blow up DAGMan
#   because of improper counting.
#
#   Author: Cole Bollig - 2023/07/21

from ornithology import *
import htcondor2 as htcondor
import os

@action
def echo_job(test_dir):
    path = os.path.join(str(test_dir), "echo.sub")
    with open(path, "w") as f:
        f.write("""# Simple echo job
executable = /bin/echo
arguments  = 'Condor is King'
universe   = local
log        = echo.log
output     = echo.out
error      = echo.err
queue
""")
    return path

@action
def fail_job(test_dir):
    script = os.path.join(str(test_dir), "fail.sh")
    with open(script, "w") as f:
        f.write("""#!/bin/bash
exit 1
""")
    os.chmod(script, 0o755)
    submit = os.path.join(str(test_dir), "fail.sub")
    with open(submit, "w") as f:
        f.write("""# Job that fails
executable = fail.sh
universe   = local
log = fail.log
queue
""")
    return submit

@action
def sleep_job(test_dir, path_to_sleep):
    path = os.path.join(str(test_dir), "sleep.sub")
    with open(path, "w") as f:
        f.write(f"""# Simple sleep job
executable = {path_to_sleep}
arguments  = 10
universe   = local
log        = sleep.log
queue
""")
    return path

@action
def submit_dag(default_condor, test_dir, echo_job, sleep_job, fail_job):
    contents = f"""JOB SLEEP {sleep_job}
SERVICE FAST {echo_job}
SERVICE FAIL {fail_job}
"""
    filename = os.path.join(str(test_dir), "test.dag")
    with open(filename, "w") as f:
        f.write(contents)
    dag = htcondor.Submit.from_dag(filename)
    job = default_condor.submit(dag)
    assert job.wait(condition=ClusterState.all_complete,timeout=80)
    return filename + ".dagman.out"

class TestDAGManServiceNodes:
    def test_short_service_doesnt_blow_up_dag(self, submit_dag):
        with open(submit_dag, "r") as f:
            for line in f:
                assert "Assertion ERROR on (_numNodesSubmitted >= 0)" not in line
                assert "Assertion ERROR on (dagman.dag->NumNodesDone( true ) + dagman.dag->NumNodesFailed() <= dagman.dag->NumNodes( true ))" not in line

