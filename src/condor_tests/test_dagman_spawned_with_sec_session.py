#!/usr/bin/env pytest

#testreq: personal
"""<<CONDOR_TESTREQ_CONFIG
	DAGMAN_DEBUG = D_SECURITY:2
"""
#endtestreq

from ornithology import *
import htcondor2
import json
from pathlib import Path

DAG_FILE = "test.dag"
TIMEOUT = 120

#-----------------------------------------------------------------------------------------
@action
def submit_dag(default_condor, path_to_sleep):
    with open(DAG_FILE, "w") as f:
        f.write(f"""
JOB sleep @=desc
    executable = {path_to_sleep}
    arguments  = 0
@desc
""")

    DAG = htcondor2.Submit.from_dag(DAG_FILE)
    return default_condor.submit(DAG)


#=========================================================================================
class TestDAGManSecSession:
    def test_dag_executed(self, submit_dag):
        # Ensure DAG runs successfully
        assert submit_dag.wait(condition=ClusterState.all_complete, timeout=TIMEOUT)
        with open(f"{DAG_FILE}.metrics", "r") as f:
            metrics = json.load(f)
            assert metrics["exitcode"] == 0

    def test_dagman_use_sec_session(self, submit_dag):
        # Check that no security session was created
        with open(f"{DAG_FILE}.dagman.out", "r") as f:
            assert "SECMAN: no cached key for" not in f.read()
