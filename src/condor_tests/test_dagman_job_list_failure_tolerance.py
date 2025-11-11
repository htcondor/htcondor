#!/usr/bin/env pytest

# test_dagman_job_list_failure_tolerance.py
#
# This test checks that the DAGMan job list fault tolerance
# works correctly for nodes

from ornithology import *
import htcondor2
import os
import json
import enum
from pathlib import Path
from shutil import rmtree
from time import time as now

TIMEOUT = 120

DAG_FILENAME = "test.dag"
DAG_CONTENTS = """
SUBMIT-DESCRIPTION chain @=desc
    executable = {exe}
    arguments  = $(ClusterId) $(ProcId) {tolerance}
    # Possible race condition with local universe may cause
    # abort event for jobs that have terminated upon DAGMan exit
    #universe   = local
    priority   = $(ProcId) * -100

    getenv     = True

    queue {count}
@desc

JOB TEST chain
"""

#------------------------------------------------------------------
# DAG Status values (keep in sync with enum in dagman_utils.h)
class DagStatus(enum.IntEnum):
    OK = 0
    ERROR = 1
    NODE_FAILED = 2
    ABORT = 3
    #RM = 4
    #CYCLE = 5
    #HALTED = 6

# Class to hold each test case:
#   - queue: # jobs to queue in list
#   - tolerance: Job failure tolerance before failure
#   - code: Expected DAGMan job exit code
#   - status: Expected DAG Status value
class TestCase:
    __test__ = False
    def __init__(self, queue: int, tolerance: int, code: int, status: DagStatus):
        self.queue = queue
        self.tolerance = tolerance
        self.exit_code = code
        self.status = status

#==================================================================
TEST_CASES = {
    "DEFAULT": TestCase(3, 0, 1, DagStatus.NODE_FAILED),
    "ONE_FAILURE": TestCase(3, 1, 1, DagStatus.NODE_FAILED),
    "FIVE_FAILURES": TestCase(10, 5, 1, DagStatus.NODE_FAILED),
    "NINE_FAILURES": TestCase(10, 9, 1, DagStatus.NODE_FAILED),
    "TOLERATE_ALL": TestCase(3, 3, 0, DagStatus.OK),
    "GREATER_THAN_NUM_PROCS": TestCase(2, 100, 0, DagStatus.OK),
}

#------------------------------------------------------------------
@action
def write_exe(test_dir):
    executable = test_dir / "chain.py"
    with open(executable, "w") as f:
        f.write(
"""#!/usr/bin/env python3

import htcondor2
import sys

FAILURE_CODE = 255
EXIT_CODE = 1

NUM_ARGS = len(sys.argv)
if NUM_ARGS != 4:
    print(f"ERROR: Invlaid number of arguments: {NUM_ARGS}")
    sys.exit(FAILURE_CODE)

try:
    CLUSTER = int(sys.argv[1])
except Exception as e:
    print(f"ERROR: Invalid ClusterId provided: {e}")
    sys.exit(FAILURE_CODE)

try:
    PROC = int(sys.argv[2])
except Exception as e:
    print(f"ERROR: Invalid ProcId provided: {e}")
    sys.exit(FAILURE_CODE)

try:
    THRES = int(sys.argv[3])
except:
    print(f"ERROR: Invalid ProcId provided: {e}")
    sys.exit(FAILURE_CODE)


if PROC == 0:
    sys.exit(EXIT_CODE)

ads = htcondor2.Schedd().query(
    constraint=f'ClusterId=={CLUSTER}&&ProcId=={PROC}',
    projection=["DAGManNodesLog"]
)

if len(ads) != 1:
    print(f"ERROR: Failed to get log(s) for job {CLUSTER}.{PROC}")
    sys.exit(FAILURE_CODE)

LOG = ads[0].get("DAGManNodesLog")

if LOG is None:
    print(f"ERROR: Job {CLUSTER}.{PROC} has no logs defined")
    sys.exit(FAILURE_CODE)

JEL = htcondor2.JobEventLog(LOG)

DONE_EVENTS = [
    htcondor2.JobEventType.JOB_TERMINATED,
    htcondor2.JobEventType.JOB_ABORTED,
]

while True:
    if PROC <= THRES:
        for event in JEL.events(stop_after=1):
            if event.cluster == CLUSTER and event.proc == (PROC - 1) and event.type in DONE_EVENTS:
                sys.exit(EXIT_CODE)
""")

    os.chmod(str(executable), 0o0777)
    return executable

#------------------------------------------------------------------
@action
def run_dags(default_condor, test_dir, write_exe):
    TESTS = dict()

    for TEST, INFO in TEST_CASES.items():
        DAG_MODIFIERS = {
            "exe" : write_exe,
            "tolerance": INFO.tolerance,
            "count": INFO.queue,
        }

        os.chdir(str(test_dir))
        case_dir = test_dir / TEST
        if case_dir.exists():
            rmtree(case_dir)
        os.mkdir(str(case_dir))
        os.chdir(str(case_dir))

        with open(DAG_FILENAME, "w") as f:
            f.write(DAG_CONTENTS.format(**DAG_MODIFIERS))

        DAG = htcondor2.Submit.from_dag(DAG_FILENAME, {"AddToEnv": f"_CONDOR_DAGMAN_NODE_JOB_FAILURE_TOLERANCE={INFO.tolerance}"})
        HANDLE = default_condor.submit(DAG)
        TESTS[TEST] = (HANDLE, INFO.tolerance, INFO.exit_code, INFO.status, INFO.queue)

    yield TESTS

#------------------------------------------------------------------
@action(params={name: name for name in TEST_CASES})
def test_info(request, run_dags, test_dir):
    return (
        request.param,               # Test name
        run_dags[request.param][0],  # Test DAG job handle
        run_dags[request.param][1],  # Test failure tolerance
        run_dags[request.param][2],  # Test expected exit code
        run_dags[request.param][3],  # Test expected DAG status
        run_dags[request.param][4],  # Test queued # of jobs
    )

@action
def test_name(test_info):
    return test_info[0]

@action
def test_handle(test_info):
    return test_info[1]

@action
def test_tolerance(test_info):
    return test_info[2]

@action
def test_exit_code(test_info):
    return test_info[3]

@action
def test_dag_status(test_info):
    return test_info[4]

@action
def test_num_procs(test_info):
    return test_info[5]

#==================================================================
# Magical class to cd into test specific dir and return to root test dir
# via pythons 'with' keyword
class ChangeDir:
    def __init__(self, switch):
        self.orig = None
        self.switch = switch
    def __enter__(self):
        self.orig = os.getcwd()
        os.chdir(self.switch)
    def __exit__(self, exec_type, exec_value, exec_traceback):
        os.chdir(self.orig)
        self.orig = None

#------------------------------------------------------------------
class TestDAGManJobFailTolerance:
    def test_dag_wait(self, test_handle):
        assert test_handle.wait(condition=ClusterState.all_complete, timeout=TIMEOUT)

    def test_dag_final_status(self, test_name, test_exit_code, test_dag_status):
        with ChangeDir(test_name):
            with open(f"{DAG_FILENAME}.metrics", "r") as f:
                metrics = json.load(f)
                assert metrics["exitcode"] == test_exit_code
                assert metrics["DagStatus"] == int(test_dag_status)

    def test_job_proc_states(self, test_name, test_tolerance, test_num_procs):
        with ChangeDir(test_name):
            LOG = Path(f"{DAG_FILENAME}.nodes.log")
            assert LOG.exists()

            START = now()
            PROCS = {
                "submitted": 0,
                "completed": 0,
            }

            with htcondor2.JobEventLog(str(LOG)) as JEL:
                while True:
                    assert now() - START < TIMEOUT
                    for event in JEL.events(stop_after=1):
                        if event.type == htcondor2.JobEventType.SUBMIT:
                            PROCS["submitted"] += 1
                            assert PROCS["submitted"] <= test_num_procs
                        elif event.type == htcondor2.JobEventType.JOB_TERMINATED:
                            assert event.proc <= test_tolerance
                            PROCS["completed"] += 1
                        elif event.type == htcondor2.JobEventType.JOB_ABORTED:
                            assert event.proc > test_tolerance
                            PROCS["completed"] += 1

                    if PROCS["completed"] == test_num_procs:
                        break

