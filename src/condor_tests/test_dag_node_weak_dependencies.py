#!/usr/bin/env pytest

#   test_dag_node_weak_dependencies.py
#
#   Regression test for WEAK PARENT/CHILD dependencies (HTCONDOR-2690).
#   A WEAK dependency only requires the parent to finish *executing*,
#   not to succeed -- unlike the default (strong) dependency already
#   covered by test_dagman_futile_nodes.py. Test groups:
#     1. Mirrors of test_dagman_futile_nodes.py's failure-injection cases,
#        with both arcs WEAK -- the immediate weak children should run
#        to completion instead of being marked FUTILE.
#     2. Non-transitivity: a node that never executes (FUTILE via a
#        strong dependency) still futilizes its own weak children.
#     3. Strongest-wins: redeclaring the same PARENT/CHILD pair as both
#        WEAK and strong always ends up strong, regardless of order.
#     4. Mixed multi-parent: a child with one weak and one strong parent.
#     5. Rescue file: failed nodes with only weak children are still
#        reported/counted as failed live, but written DONE in the Rescue DAG.

from ornithology import *
import htcondor2
import pytest
import os
import sys
import glob

from pathlib import Path
from shutil import rmtree

#-----------------------------------------------------------------------------------------
PYTHON3 = sys.executable
TIMEOUT = 300
DAG_FILENAME = "test.dag"

#-----------------------------------------------------------------------------------------
def script_simple_exit(path: Path) -> Path:
    """Write a small python3 script that exits with the given code (default 125)."""
    script = path / "exit.py"
    with open(str(script), "w") as f:
        f.write("""
import sys

CODE = 125
try:
    if len(sys.argv) >= 2:
        CODE = int(sys.argv[1])
except Exception as e:
    print(f"ERROR: Failed to convert specified exit code: {e}")

sys.exit(CODE)
""")
    return script


def write_dag_file(contents: str, name: str = DAG_FILENAME) -> str:
    with open(name, "w") as f:
        f.write(contents)
    return name


def count_node_statuses(path: str = "status.out") -> dict:
    """Parse a DAGMan NODE_STATUS_FILE and count Done/Error/Futile nodes."""
    counts = {"done": 0, "error": 0, "futile": 0}
    with open(path, "r") as f:
        for line in f.readlines():
            if "NodeStatus =" not in line:
                continue
            if "STATUS_DONE" in line:
                counts["done"] += 1
            elif "STATUS_ERROR" in line:
                counts["error"] += 1
            elif "STATUS_FUTILE" in line:
                counts["futile"] += 1
    return counts

#-----------------------------------------------------------------------------------------
# Group 1: mirrors test_dagman_futile_nodes.py's failure-injection cases, but with
# WEAK dependencies -- expect (0 done, 1 error, 2 futile) to flip to (2 done, 1 error,
# 0 futile) since B and C should now run normally instead of being futilized.

def setup_weak_post_script_failed(path: Path, path_to_sleep: str) -> str:
    exe = script_simple_exit(path)
    return write_dag_file(f"""
SUBMIT-DESCRIPTION good @=desc
    executable = {PYTHON3}
    arguments  = {exe} 0
@desc

JOB A good
JOB B good
JOB C good

SCRIPT POST A {PYTHON3} {exe} 1

WEAK PARENT A CHILD B
WEAK PARENT B CHILD C

NODE_STATUS_FILE status.out 5 ALWAYS-UPDATE
""")


def setup_weak_job_aborted(path: Path, path_to_sleep: str) -> str:
    exe = script_simple_exit(path)
    return write_dag_file(f"""
JOB A @=desc
    executable = {path_to_sleep}
    arguments  = 600
    universe   = vanilla
    periodic_remove = NumShadowStarts >= 1
    queue
@desc

SUBMIT-DESCRIPTION good @=desc
    executable = {PYTHON3}
    arguments  = {exe} 0
@desc

JOB B good
JOB C good

WEAK PARENT A CHILD B
WEAK PARENT B CHILD C

NODE_STATUS_FILE status.out 5 ALWAYS-UPDATE
""")


def setup_weak_job_failed(path: Path, path_to_sleep: str) -> str:
    exe = script_simple_exit(path)
    return write_dag_file(f"""
SUBMIT-DESCRIPTION bad @=desc
    executable = {PYTHON3}
    arguments  = {exe} 1
@desc

SUBMIT-DESCRIPTION good @=desc
    executable = {PYTHON3}
    arguments  = {exe} 0
@desc

JOB A bad
JOB B good
JOB C good

WEAK PARENT A CHILD B
WEAK PARENT B CHILD C

NODE_STATUS_FILE status.out 5 ALWAYS-UPDATE
""")


def setup_weak_failed_submission(path: Path, path_to_sleep: str) -> str:
    exe = script_simple_exit(path)
    return write_dag_file(f"""
SUBMIT-DESCRIPTION good @=desc
    executable = {PYTHON3}
    arguments  = {exe} 0
@desc

JOB A DNE.sub
JOB B good
JOB C good

WEAK PARENT A CHILD B
WEAK PARENT B CHILD C

NODE_STATUS_FILE status.out 5 ALWAYS-UPDATE
""")


def setup_weak_pre_script_failed(path: Path, path_to_sleep: str) -> str:
    exe = script_simple_exit(path)
    return write_dag_file(f"""
SUBMIT-DESCRIPTION good @=desc
    executable = {PYTHON3}
    arguments  = {exe} 0
@desc

JOB A good
JOB B good
JOB C good

SCRIPT PRE A {PYTHON3} {exe} 1

WEAK PARENT A CHILD B
WEAK PARENT B CHILD C

NODE_STATUS_FILE status.out 5 ALWAYS-UPDATE
""")

#-----------------------------------------------------------------------------------------
# Group 2: non-transitivity -- A -(strong)-> B -(weak)-> C. A fails, B is FUTILE
# (never runs), so C is also FUTILE despite the weak link -- identical result to
# an all-strong chain.

def setup_non_transitive(path: Path, path_to_sleep: str) -> str:
    exe = script_simple_exit(path)
    return write_dag_file(f"""
SUBMIT-DESCRIPTION bad @=desc
    executable = {PYTHON3}
    arguments  = {exe} 1
@desc

SUBMIT-DESCRIPTION good @=desc
    executable = {PYTHON3}
    arguments  = {exe} 0
@desc

JOB A bad
JOB B good
JOB C good

PARENT A CHILD B
WEAK PARENT B CHILD C

NODE_STATUS_FILE status.out 5 ALWAYS-UPDATE
""")

#-----------------------------------------------------------------------------------------
# Group 3: strongest-wins -- same PARENT/CHILD pair declared both WEAK and strong,
# in both orders. A fails -> B ends up FUTILE either way.

def setup_strongest_wins_weak_then_strong(path: Path, path_to_sleep: str) -> str:
    exe = script_simple_exit(path)
    return write_dag_file(f"""
SUBMIT-DESCRIPTION bad @=desc
    executable = {PYTHON3}
    arguments  = {exe} 1
@desc

SUBMIT-DESCRIPTION good @=desc
    executable = {PYTHON3}
    arguments  = {exe} 0
@desc

JOB A bad
JOB B good

WEAK PARENT A CHILD B
PARENT A CHILD B

NODE_STATUS_FILE status.out 5 ALWAYS-UPDATE
""")


def setup_strongest_wins_strong_then_weak(path: Path, path_to_sleep: str) -> str:
    exe = script_simple_exit(path)
    return write_dag_file(f"""
SUBMIT-DESCRIPTION bad @=desc
    executable = {PYTHON3}
    arguments  = {exe} 1
@desc

SUBMIT-DESCRIPTION good @=desc
    executable = {PYTHON3}
    arguments  = {exe} 0
@desc

JOB A bad
JOB B good

PARENT A CHILD B
WEAK PARENT A CHILD B

NODE_STATUS_FILE status.out 5 ALWAYS-UPDATE
""")

#-----------------------------------------------------------------------------------------
# Group 4: mixed multi-parent -- child C has one weak parent and one strong parent.

def setup_mixed_parent_weak_fails(path: Path, path_to_sleep: str) -> str:
    exe = script_simple_exit(path)
    return write_dag_file(f"""
SUBMIT-DESCRIPTION bad @=desc
    executable = {PYTHON3}
    arguments  = {exe} 1
@desc

SUBMIT-DESCRIPTION good @=desc
    executable = {PYTHON3}
    arguments  = {exe} 0
@desc

JOB A1 bad
JOB A2 good
JOB C good

WEAK PARENT A1 CHILD C
PARENT A2 CHILD C

NODE_STATUS_FILE status.out 5 ALWAYS-UPDATE
""")


def setup_mixed_parent_strong_fails(path: Path, path_to_sleep: str) -> str:
    exe = script_simple_exit(path)
    return write_dag_file(f"""
SUBMIT-DESCRIPTION bad @=desc
    executable = {PYTHON3}
    arguments  = {exe} 1
@desc

SUBMIT-DESCRIPTION good @=desc
    executable = {PYTHON3}
    arguments  = {exe} 0
@desc

JOB A1 good
JOB A2 bad
JOB C good

WEAK PARENT A1 CHILD C
PARENT A2 CHILD C

NODE_STATUS_FILE status.out 5 ALWAYS-UPDATE
""")

#-----------------------------------------------------------------------------------------
# Group 5: rescue file -- failed nodes whose only children are weak. Live run still
# reports them as failed, but the generated Rescue DAG should mark them DONE (they
# executed and satisfied every one of their weak children). Covers both the single
# child ("direct arc") and multi-child (full Edge) AllChildrenWeak() code paths.

def setup_rescue_weak_only_child(path: Path, path_to_sleep: str) -> str:
    exe = script_simple_exit(path)
    return write_dag_file(f"""
SUBMIT-DESCRIPTION bad @=desc
    executable = {PYTHON3}
    arguments  = {exe} 1
@desc

SUBMIT-DESCRIPTION good @=desc
    executable = {PYTHON3}
    arguments  = {exe} 0
@desc

JOB A bad
JOB B good

JOB D bad
JOB E good
JOB F good
JOB G good

WEAK PARENT A CHILD B
WEAK PARENT D CHILD E F G

NODE_STATUS_FILE status.out 5 ALWAYS-UPDATE
""")

#-----------------------------------------------------------------------------------------
class Details:
    def __init__(self, prep, counts: dict, check_rescue: bool = False):
        self.prepare = prep
        self.counts = counts
        self.check_rescue = check_rescue

TEST_CASES = {
    "WEAK_POST_SCRIPT_FAILED": Details(setup_weak_post_script_failed, {"done": 2, "error": 1, "futile": 0}),
    "WEAK_JOB_ABORTED": Details(setup_weak_job_aborted, {"done": 2, "error": 1, "futile": 0}),
    "WEAK_JOB_FAILED": Details(setup_weak_job_failed, {"done": 2, "error": 1, "futile": 0}),
    "WEAK_FAILED_SUBMISSION": Details(setup_weak_failed_submission, {"done": 2, "error": 1, "futile": 0}),
    "WEAK_PRE_SCRIPT_FAILED": Details(setup_weak_pre_script_failed, {"done": 2, "error": 1, "futile": 0}),

    "NON_TRANSITIVE": Details(setup_non_transitive, {"done": 0, "error": 1, "futile": 2}),

    "STRONGEST_WINS_WEAK_THEN_STRONG": Details(setup_strongest_wins_weak_then_strong, {"done": 0, "error": 1, "futile": 1}),
    "STRONGEST_WINS_STRONG_THEN_WEAK": Details(setup_strongest_wins_strong_then_weak, {"done": 0, "error": 1, "futile": 1}),

    "MIXED_PARENT_WEAK_FAILS": Details(setup_mixed_parent_weak_fails, {"done": 2, "error": 1, "futile": 0}),
    "MIXED_PARENT_STRONG_FAILS": Details(setup_mixed_parent_strong_fails, {"done": 1, "error": 1, "futile": 1}),

    "RESCUE_WEAK_ONLY_CHILD": Details(setup_rescue_weak_only_child, {"done": 4, "error": 2, "futile": 0}, check_rescue=True),
}

#-----------------------------------------------------------------------------------------
@action
def run_dags(default_condor, test_dir, path_to_sleep):
    assert PYTHON3 is not None

    TESTS = dict()

    for TEST, DETAILS in TEST_CASES.items():
        case_dir = test_dir / TEST
        if case_dir.exists():
            rmtree(case_dir)
        os.mkdir(case_dir)

        with ChangeDir(TEST):
            dag_file = DETAILS.prepare(case_dir, path_to_sleep)

            with default_condor.use_config():
                dag = htcondor2.Submit.from_dag(dag_file)

            TESTS[TEST] = default_condor.submit(dag)

    yield TESTS

#-----------------------------------------------------------------------------------------
@action(params={name: name for name in TEST_CASES})
def test_case(request) -> str:
    return request.param

@action
def test_details(test_case) -> Details:
    return TEST_CASES[test_case]

@action
def test_dag_handle(test_case, run_dags):
    handle = run_dags[test_case]
    assert handle.wait(condition=ClusterState.all_complete, timeout=TIMEOUT)
    return handle

#=========================================================================================
class TestDagManWeakDependencies:
    def test_node_statuses(self, test_case, test_details, test_dag_handle):
        with ChangeDir(test_case):
            counts = count_node_statuses()

        assert counts == test_details.counts

    def test_rescue_file(self, test_case, test_details, test_dag_handle):
        if not test_details.check_rescue:
            pytest.skip("Test case does not check for a Rescue DAG")

        with ChangeDir(test_case):
            rescues = sorted(glob.glob(f"{DAG_FILENAME}.rescue*"))
            assert rescues, "No Rescue DAG file was generated"

            with open(rescues[-1], "r") as f:
                contents = f.read()

        assert "DONE A" in contents
        assert "DONE D" in contents
