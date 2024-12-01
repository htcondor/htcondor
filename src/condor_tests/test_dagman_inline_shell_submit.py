#!/usr/bin/env pytest

#   test_dagman_inline_shell_submit.py
#
#   This test multiple functionalities in regards to DAGMan
#   inline submit descriptions. This includs:
#       1. Works with shell condor_submit
#       2. JOB sub commands work correctly (DIR, DONE, NOOP)
#       3. Support for lists of jobs (queue N)
#       4. Support for '{}' or '@=' enclosing operators
#       5. Support for prepend of job VARS
#       6. Inline works for FINAL, PROVISIONER, & SERVICE nodes
#
#   Author: Cole Bollig - 2024/09/11

from ornithology import *
import htcondor2
import os
from shutil import rmtree
import re

SUBDIR = "test_subdir"
JOB_LOG = "check.log"
D_CLUSTER = -1

TEST_CASES = {
    "SHELL_SUBMIT" : {
        "contents" : """
# Node A: Queue 1 && Success
JOB A {{
    executable = {sleep_exe}
    arguments  = 0
}}

# Node B: Should be marked as DONE else DAG fails
JOB B @=EndB
    executable = {exit_exe}
    arguments  = 1
@EndB DONE

# Node C: Should be NOOP or else DAG fails waiting
JOB C @=C-DESC
    executable = {sleep_exe}
    arguments  = 360
@C-DESC NOOP

# Node D: Queue 2 && Success && run in sub-directory
JOB D @=blah
    executable = {sleep_exe}
    arguments  = 0
    log = {log}
    queue 2
@blah DIR {dir}""",
        "SubmitMethod" : 0,
        "TotalNodes" : 4,
        "SubmittedJobs" : 3,
    },

    "PREPEND_VARS" : {
        "contents" : """
JOB CUSTOM_VAR_NODE @=prepend-vars-test
    executable = {sleep_exe}
    arguments  = 0
    if defined CUSTOM_VAR
        My.HasCustomVar = True
    else
        My.HasCustomVar = False
    endif
@prepend-vars-test

VARS CUSTOM_VAR_NODE PREPEND CUSTOM_VAR="True" """,
        "SubmitMethod" : 1,
        "TotalNodes" : 1,
        "SubmittedJobs" : 1,
    },

    "SPECIAL_NODES" : {
        "contents" : """
PROVISIONER P {{
    executable = {exit_exe}
    arguments  = 0
}}

SERVICE S @=my-service-node
    executable = {exit_exe}
    arguments  = 0
    queue 3
@my-service-node

FINAL F @=final-node
    executable = {exit_exe}
    arguments  = 0
@final-node
        """,
        "SubmitMethod" : 0,
        "TotalNodes" : 2, # Service nodes don't count towards node count
        "SubmittedJobs" : 5,
    }
}

#==================================================================
@standup
def condor(test_dir):
    with Condor(local_dir=test_dir / "condor", config={
            "DAGMAN_REMOVE_TEMP_SUBMIT_FILES" : False}) as condor:
        yield condor

#------------------------------------------------------------------
@action
def write_exit_script(test_dir):
    script = test_dir / "exit.sh"
    with open(script, "w") as f:
        f.write("#!/bin/bash\nexit $1\n")
        os.chmod(script, 0o755)
        return script

#------------------------------------------------------------------
@action
def run_dags(condor, test_dir, path_to_sleep, write_exit_script):
    if os.path.exists(SUBDIR):
        rmtree(SUBDIR)
    os.mkdir(SUBDIR)

    test_info = dict()

    fmt = {
        "sleep_exe" : path_to_sleep,
        "exit_exe" : write_exit_script,
        "log" : JOB_LOG,
        "dir" : SUBDIR,
    }

    for test, details in TEST_CASES.items():
        dag_file = test + ".dag"
        with open(dag_file, "w") as f:
            f.write(details["contents"].format(**fmt))
        dag = htcondor2.Submit.from_dag(dag_file, {"SubmitMethod" : details["SubmitMethod"]})
        handle = condor.submit(dag)
        test_info[test] = (handle, details["TotalNodes"], details["SubmittedJobs"])

    yield test_info

#------------------------------------------------------------------
@action(params={name: name for name in TEST_CASES})
def test_info(request, run_dags):
    return (request.param, run_dags[request.param][0], run_dags[request.param][1], run_dags[request.param][2])

@action
def test_name(test_info):
    return test_info[0]

@action
def test_dag(test_info):
    return test_info[1]

@action
def dag_wait(test_dag):
    assert test_dag.wait(condition=ClusterState.all_complete, timeout=45)
    return test_dag

@action
def dag_num_nodes(test_info):
    return test_info[2]

@action
def dag_num_jobs(test_info):
    return test_info[3]

#------------------------------------------------------------------
@action
def test_get_history(condor, dag_wait, dag_num_nodes, dag_num_jobs):
    ads = list()
    const = f'(ClusterId=={dag_wait.clusterid} || DAGManJobId=={dag_wait.clusterid})'
    proj = ["DAGNodeName", "DAG_NodesTotal", "ClusterId", "HasCustomVar", "ExitCode"]
    with condor.use_config():
        ads = htcondor2.Schedd().history(constraint=const, projection=proj)
    return (ads, dag_num_nodes, dag_num_jobs)

#==================================================================
class TestDAGManInlineDesc:
    def test_check_job_history(self, test_get_history):
        global D_CLUSTER
        num_node_jobs = 0
        num_dag_jobs = 0
        for ad in test_get_history[0]:
            if "DAGNodeName" not in ad:
                num_dag_jobs += 1
                assert ad.get("DAG_NodesTotal") == test_get_history[1]
                assert ad["ExitCode"] == 0
            else:
                num_node_jobs += 1
                if ad["DAGNodeName"] == "CUSTOM_VAR_NODE":
                    assert ad.get("HasCustomVar") == True
                else:
                    assert ad.get("HasCustomVar") == None
                    D_CLUSTER = ad["ClusterId"] if ad["DAGNodeName"] == "D" else D_CLUSTER

        assert num_node_jobs == test_get_history[2]
        assert num_dag_jobs == 1

    def test_check_temp_files(self):
        inline_files = set()
        for path in [".", SUBDIR]:
            inline_files.update([f for f in os.listdir(path) if len(f) > 5 and f[-5:] == ".temp"])

        for f in inline_files:
            assert re.match("^[ADFSP]{1}-inline.[0-9]+.temp", f)

    def test_check_multi_queue_log(self):
        expected_procs = set([0, 1, 2])
        log = os.path.join(SUBDIR, JOB_LOG)
        assert os.path.exists(log)
        jel = htcondor2.JobEventLog(log)
        for event in jel.events(0):
            assert event.cluster == D_CLUSTER
            assert event.proc in expected_procs

