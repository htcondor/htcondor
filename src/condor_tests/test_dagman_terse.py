#!/usr/bin/env pytest

from ornithology import *
import os
import re

#-------------------------------------------------------------------------------------------------------
#Actually submit the DAG with -terse: stdout should be just the job id line
@action
def submit_dag_terse(default_condor, test_dir):
    dag_path = test_dir / "terse.dag"
    with open(dag_path, "w") as f:
        f.write("JOB A DUMMY NOOP\n")

    p = default_condor.run_command(["condor_submit_dag", "-terse", dag_path])
    #Clean up the job(s) we just submitted so they don't linger
    default_condor.run_command(["condor_rm", "-all"])

    return p

#=======================================================================================================
class TestCondorSubmitDagTerse:
    def test_terse_submit_prints_only_jobid(self, submit_dag_terse):
        assert submit_dag_terse.returncode == 0
        output = submit_dag_terse.stdout.strip()
        assert re.match(r"^\d+\.\d+ - \d+\.\d+$", output), f"Unexpected -terse output: {output!r}"
