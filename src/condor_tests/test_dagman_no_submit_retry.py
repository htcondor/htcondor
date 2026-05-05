#!/usr/bin/env pytest

#------------------------------------------------------------------
# This test verifies that DAGMan does not retry job submission for
# submission failures that will never work (and we can verify).
# Note: This only applies for DAGs using direct submit
#------------------------------------------------------------------

#testreq: personal
"""<<CONDOR_TESTREQ_CONFIG
    SUBMIT_REQUIREMENT_NAMES = NoMalicousJobs
    SUBMIT_REQUIREMENT_NoMalicousJobs = MalicousJob =!= True
    DAGMAN_MAX_SUBMIT_ATTEMPTS = 2
    DAGMAN_DEBUG = D_TEST D_CAT
"""
#endtestreq

from ornithology import *
import htcondor2
import re
import sys
from pathlib import Path

#------------------------------------------------------------------
PYTHON3 = sys.executable
TEST_CASES = {
    "ALL": "test.dag",
    "NO_MULTI_PROC": "no-multi-proc.dag",
}

#------------------------------------------------------------------
@action
def the_extra_jdl(test_dir) -> Path:
    extra_bits = test_dir / "extra.sub"
    with open(extra_bits, "w") as f:
        f.write("on_exit_hold = ExitCode == 100")

    return extra_bits

#------------------------------------------------------------------
@action
def the_abort_script(test_dir) -> Path:
    script = test_dir / "abort_dag.py"
    with open(script, "w") as f:
        f.write(
"""# Script that exits with code to make DAG abort in specific way
import sys
sys.exit(123)
"""
        )

    return script

#------------------------------------------------------------------
@action
def the_no_multi_job_dag(test_dir, the_abort_script) -> Path:
    dag_file = test_dir / TEST_CASES["NO_MULTI_PROC"]
    with open(dag_file, "w") as f:
        f.write(
f"""
# DAG to check prohibit multi proc jobs case
ENV SET _CONDOR_DAGMAN_PROHIBIT_MULTI_JOBS=true
ABORT-DAG-ON ALL_NODES 123 RETURN 100

JOB MULTI-PROC @=desc
    executable = {PYTHON3}
    arguments  = {the_abort_script}
    queue 10
@desc
"""
        )

    return dag_file

#------------------------------------------------------------------
@action
def the_dag(test_dir, the_abort_script) -> Path:
    dag_file = test_dir / TEST_CASES["ALL"]
    with open(dag_file, "w") as f:
        f.write(
f"""
# DAG filled with job submission failures (direct submit)
# that should not be retried
# NOTE: All jobs use abort script in case jobs are submitted
# successfully

ABORT-DAG-ON ALL_NODES 123 RETURN 100

JOB MISSING-JDL-FILE dne.sub

JOB FAIL-PARSE-TO-Q-LINE @=desc
    executable = {PYTHON3}
    arguments  = {the_abort_script}
    JOB stray_dag_keyword dne.sub
    queue
@desc

JOB Q-OOR @=desc
    executable = {PYTHON3}
    arguments  = {the_abort_script}
    queue -5
@desc

JOB Q-BAD-IN-PAIR @=desc
    executable = {PYTHON3}
    arguments  = {the_abort_script}
    queue item in files (dne1.txt dne2.txt)
@desc

JOB Q-INVALID-SLICE @=desc
    executable = {PYTHON3}
    arguments  = {the_abort_script}
    queue item[a:b] in (item1 item2)
@desc

JOB Q-BAD-TABLE-OPT @=desc
    executable = {PYTHON3}
    arguments  = {the_abort_script}
    queue col from table(badopt=5) dne.txt
@desc

JOB Q-FROM-FILE-DNE @=desc
    executable = {PYTHON3}
    arguments  = {the_abort_script}
    queue item from dne.txt
@desc

JOB Q-MISSING-END-PAREN @=desc
    executable = {PYTHON3}
    arguments  = {the_abort_script}
    queue item in (item1
    item2
    # end of description; missing -> )
@desc

JOB FAIL-SUBMIT-REQ @=desc
    executable = {PYTHON3}
    arguments  = {the_abort_script}
    My.MalicousJob = True
    queue
@desc

JOB BAD-JDL-COMMAND @=desc
    #executable = {PYTHON3}
    arguments  = {the_abort_script}
    queue
@desc
"""
        )

    return dag_file

@action
def the_dags(the_dag, the_no_multi_job_dag) -> tuple:
    return (the_dag, the_no_multi_job_dag)

#------------------------------------------------------------------
@action
def run_dags(default_condor, the_dags, the_extra_jdl):
    TESTS = {}
    for dag_file in the_dags:
        DAG = htcondor2.Submit.from_dag(str(dag_file), {"AppendFile": str(the_extra_jdl)})
        HANDLE = default_condor.submit(DAG)

        for key, val in TEST_CASES.items():
            if dag_file.name == val:
                TESTS[key] = (dag_file, HANDLE)
                break

    yield TESTS


#------------------------------------------------------------------
@action(params={name: name for name in TEST_CASES})
def test_info(request, run_dags) -> tuple:
    return (request.param, run_dags[request.param][0], run_dags[request.param][1])

@action
def test_name(test_info) -> str:
    return test_info[0]

@action
def test_dag(test_info) -> Path:
    return test_info[1]

@action
def test_handle(test_info):
    return test_info[2]

#==================================================================
class TestDAGManSubmitFailure:
    def test_dag_executes(self, test_handle):
        assert test_handle.wait(
            condition=ClusterState.all_complete,
            fail_condition=ClusterState.any_held,
            timeout=30
        )

    def test_dag_only_attempts_submit_once(self, test_dag):
        log = DaemonLog(test_dag.with_suffix(test_dag.suffix + ".dagman.out"))
        stream = log.open()
        for details in stream.read():
            if "D_TEST" in details.tags:
                assert not details.message.endswith("RETRYING NODE SUBMISSION")
