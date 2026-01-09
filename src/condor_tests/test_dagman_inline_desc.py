#!/usr/bin/env pytest

#   test_dagman_inline_desc.py
#
#   This test that inline submit description
#   have the following behavior:
#       A. Ordering doesn't matter (i.e. can occur post JOB command)
#       B. Inlcude files can 

from ornithology import *
import htcondor2
import json

DAG_FILE = "test.dag"

#------------------------------------------------------------------
@action
def write_include_file(test_dir, path_to_sleep):
    include = test_dir / "dag.include"
    with open(str(include), "w") as f:
        f.write(
f"""
SUBMIT-DESCRIPTION INC @=desc
    executable = {path_to_sleep}
    arguments  = 0
@desc
""")
    return include

#------------------------------------------------------------------
@action
def write_splice(test_dir, path_to_sleep):
    splice = test_dir / "splice.dag"
    with open(str(splice), "w") as f:
        f.write(
f"""
SUBMIT-DESCRIPTION foo @=desc
    executable = {path_to_sleep}
    arguments  = 0
@desc

# Note: This should not take effect (or test will fail)
SUBMIT-DESCRIPTION sleep @=desc
    executable = {path_to_sleep}
    arguments  = 1000000
@desc

JOB A foo
JOB B sleep
JOB C @=desc
    executable = {path_to_sleep}
    arguments  = 0
@desc
""")
    return splice

#------------------------------------------------------------------
@action
def write_dag(test_dir, path_to_sleep, write_include_file, write_splice):
    dag = test_dir / DAG_FILE
    with open(str(dag), "w") as f:
        f.write(
f"""
JOB BEFORE sleep

SUBMIT-DESCRIPTION sleep @=desc
    executable = {path_to_sleep}
    arguments  = 0
@desc

JOB AFTER sleep
JOB INCLUDE INC

SPLICE A {write_splice}

INCLUDE {write_include_file}
""")
    return dag

#------------------------------------------------------------------
@action
def run_dag(request, default_condor, write_dag):
    dag = htcondor2.Submit.from_dag(str(write_dag))
    return default_condor.submit(dag)

#==================================================================
class TestDAGManInlinePosition:
    def test_dag_ran(self, run_dag):
        assert run_dag.wait(condition=ClusterState.all_complete, timeout=120)
        with open(DAG_FILE + ".metrics", "r") as f:
            stats = json.load(f)
            assert stats["exitcode"] == 0
