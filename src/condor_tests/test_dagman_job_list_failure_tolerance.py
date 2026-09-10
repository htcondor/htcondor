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
    arguments  = $(ClusterId) $(ProcId) {tolerance} {natural_complete}
    # Possible race condition with local universe may cause
    # abort event for jobs that have terminated upon DAGMan exit
    #universe   = local
    priority   = $(ProcId) * -100

    getenv     = True

    queue {count}
@desc

JOB TEST chain
{tolerance_cmd}"""

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
#   - tolerance: Job failure tolerance before failure (a percent value when percentage=True)
#   - code: Expected DAGMan job exit code
#   - status: Expected DAG Status value
#   - mode: Per-node TOLERANCE command override (None, "WAIT", or "FAIL_FAST")
#   - remove_on_failure: DAGMAN_REMOVE_JOB_LIST_ON_FAILURE override (None = leave at default)
#   - expect_removal: Whether procs past the tolerance are expected to be condor_rm'd
#     once the node's job list is declared failed (i.e. Node::RemoveOnBatchFailure())
#   - percentage: Whether `tolerance` is a percent value (appends '%' in the TOLERANCE command)
#   - exe_threshold: Actual proc-index failure count chain.py should simulate; defaults to
#     `tolerance`, but must be given explicitly when `tolerance` is a percentage, since
#     that's a raw '%' value, not the resolved proc-count threshold
class TestCase:
    __test__ = False
    def __init__(self, queue: int, tolerance: int, code: int, status: DagStatus,
                 mode: str = None, remove_on_failure: bool = None, expect_removal: bool = True,
                 percentage: bool = False, exe_threshold: int = None):
        self.queue = queue
        self.tolerance = tolerance
        self.exit_code = code
        self.status = status
        self.mode = mode
        self.remove_on_failure = remove_on_failure
        self.expect_removal = expect_removal
        self.percentage = percentage
        self.exe_threshold = tolerance if exe_threshold is None else exe_threshold

#==================================================================
TEST_CASES = {
    "DEFAULT": TestCase(3, 0, 1, DagStatus.NODE_FAILED),
    "ONE_FAILURE": TestCase(3, 1, 1, DagStatus.NODE_FAILED),
    "FIVE_FAILURES": TestCase(10, 5, 1, DagStatus.NODE_FAILED),
    "NINE_FAILURES": TestCase(10, 9, 1, DagStatus.NODE_FAILED),
    "TOLERATE_ALL": TestCase(3, 3, 0, DagStatus.OK),
    "GREATER_THAN_NUM_PROCS": TestCase(2, 100, 0, DagStatus.OK),
    # Cases below exercise Node::RemoveOnBatchFailure(): a per-node TOLERANCE
    # mode, or the DAGMAN_REMOVE_JOB_LIST_ON_FAILURE config knob, decide whether
    # the rest of a failed node's job list gets condor_rm'd.
    "TOLERANCE_WAIT_OVERRIDE": TestCase(5, 1, 1, DagStatus.NODE_FAILED,
                                         mode="WAIT", expect_removal=False),
    "CONFIG_DISABLED_NO_REMOVE": TestCase(5, 1, 1, DagStatus.NODE_FAILED,
                                           remove_on_failure=False, expect_removal=False),
    "FAIL_FAST_FORCES_REMOVE": TestCase(5, 1, 1, DagStatus.NODE_FAILED,
                                         mode="FAIL_FAST", remove_on_failure=False, expect_removal=True),
    # 5 jobs at 20% tolerance -> (5 * 20) / 100 == 1, the same effective
    # threshold as ONE_FAILURE, but arrived at via percentage math instead of
    # a literal count. Regression check for Node::CheckBatchFailed()'s percent
    # path, which previously divided before multiplying and truncated to 0
    # for any job list under 100 jobs.
    "PERCENTAGE_TOLERANCE": TestCase(5, 20, 1, DagStatus.NODE_FAILED,
                                      percentage=True, exe_threshold=1),
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
if NUM_ARGS not in (4, 5):
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

# Procs past THRES either hang forever waiting to be condor_rm'd (default,
# matching the old always-remove behavior) or, if NATURAL_COMPLETE is set,
# succeed on their own -- used to prove the job list was NOT removed.
NATURAL_COMPLETE = NUM_ARGS == 5 and sys.argv[4] == "1"


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
    elif NATURAL_COMPLETE:
        for event in JEL.events(stop_after=1):
            if event.cluster == CLUSTER and event.proc == (PROC - 1) and event.type in DONE_EVENTS:
                sys.exit(0)
""")

    os.chmod(str(executable), 0o0777)
    return executable

#------------------------------------------------------------------
@action
def run_dags(default_condor, test_dir, write_exe):
    TESTS = dict()

    for TEST, INFO in TEST_CASES.items():
        # TOLERANCE line only needed when testing a per-node mode override or a
        # percentage value (the global env-var fallback is int-only); natural
        # completion only needed when procs past tolerance must NOT be
        # removed, so they have to be able to finish on their own.
        tolerance_cmd = ""
        if INFO.mode or INFO.percentage:
            value = f"{INFO.tolerance}%" if INFO.percentage else str(INFO.tolerance)
            mode_kw = f" {INFO.mode}" if INFO.mode else ""
            tolerance_cmd = f"TOLERANCE TEST {value}{mode_kw}\n"
        DAG_MODIFIERS = {
            "exe" : write_exe,
            "tolerance": INFO.exe_threshold,
            "count": INFO.queue,
            "tolerance_cmd": tolerance_cmd,
            "natural_complete": 0 if INFO.expect_removal else 1,
        }

        os.chdir(str(test_dir))
        case_dir = test_dir / TEST
        if case_dir.exists():
            rmtree(case_dir)
        os.mkdir(str(case_dir))
        os.chdir(str(case_dir))

        with open(DAG_FILENAME, "w") as f:
            f.write(DAG_CONTENTS.format(**DAG_MODIFIERS))

        # Skip the global tolerance override when a per-node TOLERANCE command
        # is present (mode or percentage): Node::CheckBatchFailed() ignores the
        # global value entirely once a per-node tolerance is set, so setting
        # both would let the test pass "by coincidence" if the per-node
        # override silently failed to apply.
        env_vars = []
        if INFO.mode is None and not INFO.percentage:
            env_vars.append(f"_CONDOR_DAGMAN_NODE_JOB_FAILURE_TOLERANCE={INFO.tolerance}")
        if INFO.remove_on_failure is not None:
            env_vars.append(f"_CONDOR_DAGMAN_REMOVE_JOB_LIST_ON_FAILURE={INFO.remove_on_failure}")

        # AddToEnv takes a single ';'-delimited key=value string, not a list
        options = {"AddToEnv": ";".join(env_vars)} if env_vars else {}
        DAG = htcondor2.Submit.from_dag(DAG_FILENAME, options)
        HANDLE = default_condor.submit(DAG)
        TESTS[TEST] = (HANDLE, INFO.exe_threshold, INFO.exit_code, INFO.status, INFO.queue, INFO.expect_removal)

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
        run_dags[request.param][5],  # Whether job list removal is expected
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

@action
def test_expect_removal(test_info):
    return test_info[6]

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

    def test_job_proc_states(self, test_name, test_tolerance, test_num_procs, test_expect_removal):
        with ChangeDir(test_name):
            LOG = Path(f"{DAG_FILENAME}.nodes.log")
            assert LOG.exists()

            START = now()
            submitted = 0
            terminated = set()
            aborted = set()

            with htcondor2.JobEventLog(str(LOG)) as JEL:
                while True:
                    assert now() - START < TIMEOUT
                    for event in JEL.events(stop_after=1):
                        if event.type == htcondor2.JobEventType.SUBMIT:
                            submitted += 1
                            assert submitted <= test_num_procs
                        elif event.type == htcondor2.JobEventType.JOB_TERMINATED:
                            if test_expect_removal:
                                # Procs at or below the tolerance index are the
                                # ones whose failures DAGMan tolerates (or the
                                # last one whose failure exceeds the tolerance,
                                # triggering the abort of the remaining procs).
                                assert event.proc <= test_tolerance
                            # else: Node::RemoveOnBatchFailure() is expected to
                            # return false here, so procs past the tolerance are
                            # left alone and allowed to terminate on their own.
                            terminated.add(event.proc)
                        elif event.type == htcondor2.JobEventType.JOB_ABORTED:
                            assert test_expect_removal
                            # When DAGMan reaches the failure tolerance limit,
                            # it issues a condor_rm on the whole cluster. That
                            # condor_rm can race with the terminate event for
                            # the last failing proc, causing the schedd to emit
                            # a JOB_ABORTED event for a proc that has already
                            # terminated. Accept that race: only check the
                            # proc-index invariant for procs we have not seen
                            # a terminate event for.
                            if event.proc not in terminated:
                                assert event.proc > test_tolerance
                            aborted.add(event.proc)

                    # Each proc reaches a terminal state via either a terminate
                    # or abort event (and possibly both, due to the race above).
                    if len(terminated | aborted) == test_num_procs:
                        break

