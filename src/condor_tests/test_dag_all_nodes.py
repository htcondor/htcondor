#!/usr/bin/env pytest

#testreq: personal
"""<<CONDOR_TESTREQ_CONFIG
	DAGMAN_DEBUG = D_TEST D_CAT
"""
#endtestreq

from ornithology import *
import htcondor2
import pytest
import os
import sys
import json

from time import sleep
from pathlib import Path
from shutil import rmtree
from typing import Callable, Tuple, Any

#-----------------------------------------------------------------------------------------
PYTHON3 = sys.executable
TIMEOUT = 300
DAG_FILENAME = "test.dag"

#-----------------------------------------------------------------------------------------
def script_check_retry(path):
    script = path / "check_retry.py"
    with open(str(script), "w") as f:
        f.write("""
import sys

EXPECTED = int(sys.argv[1])
RETRY_MAX = int(sys.argv[2])

if EXPECTED != RETRY_MAX:
    sys.exit(125)

sys.exit(0)
""")

    return script


def script_simple_exit(path):
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

#-----------------------------------------------------------------------------------------
def setup_special_nodes(path: Path) -> str:
    exe = script_simple_exit(path)
    return write_dag_file(f"""
SUBMIT-DESCRIPTION test @=desc
    executable = {PYTHON3}
    arguments  = {exe} $(code)
@desc

JOB A test
SERVICE S test
FINAL F test

VARS S code=0
VARS F code=0

# This should not overwrite special node types
VARS ALL_NODES code=1

# Ensure worker node does not fail
VARS A code=0

ABORT-DAG-ON S 1
""")

#-----------------------------------------------------------------------------------------
def setup_all_nodes_vars(path: Path) -> str:
    exe = script_simple_exit(path)
    return write_dag_file(f"""
SUBMIT-DESCRIPTION test @=desc
    executable = {PYTHON3}
    arguments  = {exe} $(code)
    transfer_executable = false
    queue 1
@desc

JOB A test
JOB B test
JOB C test

VARS A code=1
VARS B code=1
VARS C code=1

# Override VARS code variable so jobs execute successfully
VARS ALL_NODES code=0
""")

#-----------------------------------------------------------------------------------------
def setup_all_nodes_scripts(path: Path) -> str:
    exe = script_simple_exit(path)
    return write_dag_file(f"""

SUBMIT-DESCRIPTION test @=desc
    executable = {PYTHON3}
    arguments  = {exe} 0
    transfer_executable = false
    queue 1
@desc

JOB A test
JOB B test
JOB C test

# Pre script that fails for all nodes
SCRIPT PRE A {PYTHON3} {exe} 1
SCRIPT PRE B {PYTHON3} {exe} 1
SCRIPT PRE C {PYTHON3} {exe} 1

# Post script that fails for all nodes
SCRIPT POST A {PYTHON3} {exe} 1
SCRIPT POST B {PYTHON3} {exe} 1
SCRIPT POST C {PYTHON3} {exe} 1

# This should override all pre/post scripts to succeed
SCRIPT PRE ALL_NODES {PYTHON3} {exe} 0
SCRIPT POST ALL_NODES {PYTHON3} {exe} 0
""")

#-----------------------------------------------------------------------------------------
def setup_all_nodes_pre_skip(path: Path) -> str:
    exe = script_simple_exit(path)
    return write_dag_file(f"""
SUBMIT-DESCRIPTION test @=desc
    executable = {PYTHON3}
    arguments  = {exe} 0
    transfer_executable = false
    queue 1
@desc

JOB A test
JOB B test
JOB C test

# Pre script that fails for all nodes
SCRIPT PRE A {PYTHON3} {exe} 10
SCRIPT PRE B {PYTHON3} {exe} 10
SCRIPT PRE C {PYTHON3} {exe} 10

# Inform DAG to skip node if script exit 10
PRE_SKIP ALL_NODES 10
""")

#-----------------------------------------------------------------------------------------
def setup_all_nodes_retry(path: Path) -> str:
    script = script_check_retry(path)
    exe = script_simple_exit(path)
    return write_dag_file(f"""
SUBMIT-DESCRIPTION test @=desc
    executable = {PYTHON3}
    arguments  = {exe} 0
    transfer_executable = false
    queue 1
@desc

JOB A test
JOB B test
JOB C test

# Add post script to check set retry value
SCRIPT POST ALL_NODES {PYTHON3} {script} 10 $MAX_RETRIES

# Add retry value that will fail post script always
RETRY A 2 UNLESS-EXIT 125
RETRY B 2 UNLESS-EXIT 125
RETRY C 2 UNLESS-EXIT 125

# Override retry value to pass post script
RETRY ALL_NODES 10 UNLESS-EXIT 125
""")

#-----------------------------------------------------------------------------------------
def setup_all_nodes_abort_dag_on(path: Path) -> str:
    exe = script_simple_exit(path)
    return write_dag_file(f"""
JOB A @=desc
    executable = {PYTHON3}
    arguments  = {exe} 42
    transfer_executable = false
@desc

# Note can't easily check multiple node settings
ABORT-DAG-ON ALL_NODES 42 RETURN 2
""")

#-----------------------------------------------------------------------------------------
def setup_all_nodes_priority(path: Path) -> str:
    exe = script_simple_exit(path)
    return write_dag_file(f"""
SUBMIT-DESCRIPTION test @=desc
    executable = {PYTHON3}
    arguments  = {exe} 0
    transfer_executable = false
@desc

JOB A test
JOB B test
JOB C test

# Node priorities that will fail test
PRIORITY A 25
PRIORITY B 25
PRIORITY C 50

# Override node priorities with passing value
PRIORITY ALL_NODES 100
""")

def setup_all_nodes_category(path: Path) -> str:
    exe = script_simple_exit(path)
    return write_dag_file(f"""
SUBMIT-DESCRIPTION test @=desc
    executable = {PYTHON3}
    arguments  = {exe} 0
    transfer_executable = false
@desc

JOB A test
JOB B test
JOB C test

# Add all nodes into ALL category (expect 3 nodes in check)
CATEGORY ALL_NODES ALL
""")


#-----------------------------------------------------------------------------------------
def setup_all_nodes_splice(path: Path) -> str:
    exe = script_simple_exit(path)

    include = write_dag_file(f"""
SUBMIT-DESCRIPTION test @=desc
    executable = {PYTHON3}
    arguments  = {exe} $(code)
    transfer_executable = false
@desc
""", "job-desc.inc")

    splice = write_dag_file(f"""
INCLUDE {include}

JOB A test

# Ensure ALL_NODES stays within this splice
VARS ALL_NODES code=99
ABORT-DAG-ON ALL_NODES 99 RETURN 2
CATEGORY ALL_NODES SPLICED
""", "splice.dag")

    return write_dag_file(f"""
INCLUDE {include}

JOB A test
VARS A code=0

SPLICE S {splice}
""")

#-----------------------------------------------------------------------------------------
def verify_prio(condor, test_case, dag_handle) -> dict:
    schedd = condor.get_local_schedd()

    attempts = 0
    history = []

    while len(history) != 3:
        if attempts > 0:
            sleep(2)

        attempts += 1
        assert attempts <= 10, f"ERROR: Failed to get history all expected job records for test case {test_case}"
        history = schedd.history(
            constraint=f'DAGManJobId=={dag_handle.clusterid}',
            projection=["JobPrio"],
        )

    results = dict()
    for ad in history:
        prio = ad.get("JobPrio")
        if prio in results:
            results[prio] += 1
        else:
            results.update({prio : 1})

    return results

#-----------------------------------------------------------------------------------------
def verify_categories(condor, test_case, dag_handle) -> dict:
    categories = dict()

    KEY = "CHECK CATEGORY"

    log = DaemonLog(Path(f"{DAG_FILENAME}.dagman.out"))
    stream = log.open()
    for details in stream.read():
        if "D_TEST" in details.tags:
            line = str(details)
            # Line: <timestamp> (D_TEST) CHECK CATEGORY TotalNodes ThrottleLimit
            cat, num_nodes, _ = line[line.find(KEY) + len(KEY):].split()
            assert cat not in categories
            categories[cat] = int(num_nodes)

    return categories

#-----------------------------------------------------------------------------------------
class Details():
    def __init__(self, prep: Callable[[Path], str], ec: int, verifier: Tuple[Callable[[Condor, str, Handle], Any], Any] = None) -> None:
        self.prepare = prep
        self.exit_code = ec
        self.verification = verifier

TEST_CASES = {
    "VARS": Details(setup_all_nodes_vars, 0),
    "SCRIPTS": Details(setup_all_nodes_scripts, 0),
    "PRE_SKIP": Details(setup_all_nodes_pre_skip, 0),
    "RETRY": Details(setup_all_nodes_retry, 0),
    "ABORT_DAG_ON": Details(setup_all_nodes_abort_dag_on, 2),
    "PRIORITY": Details(setup_all_nodes_priority, 0, (verify_prio, {100:3})),
    "CATEGORY": Details(setup_all_nodes_category, 0, (verify_categories, {"ALL":3})),
    "SPLICE": Details(setup_all_nodes_splice, 2, (verify_categories, {"S+SPLICED":1})),
    "IGNORE_SPECIAL_NODES": Details(setup_special_nodes, 0),
}

#-----------------------------------------------------------------------------------------
@action
def run_dags(default_condor, test_dir):
    assert PYTHON3 is not None

    TESTS = dict()

    for TEST, SETUP in TEST_CASES.items():
        case_dir = test_dir / TEST
        if case_dir.exists():
            rmtree(case_dir)

        os.mkdir(case_dir)

        with ChangeDir(TEST):
            DAG = htcondor2.Submit.from_dag(SETUP.prepare(case_dir))

            HANDLE = default_condor.submit(DAG)
            TESTS[TEST] = (HANDLE, SETUP.exit_code, SETUP.verification)

    yield TESTS

#-----------------------------------------------------------------------------------------
@action(params={name: name for name in TEST_CASES})
def test_info(request, run_dags):
    TEST = run_dags[request.param]

    return (
        request.param,  # Test case name
        TEST[0],        # Test case job handle
        TEST[1],        # Test case expected DAG exit code
        TEST[2],        # Test case specific verifier
    )

@action
def test_case(test_info):
    return test_info[0]

@action
def test_dag_handle(test_info):
    HANDLE = test_info[1]
    assert HANDLE.wait(condition=ClusterState.all_complete, timeout=TIMEOUT)
    return HANDLE

@action
def test_expected_dag_ec(test_info, test_dag_handle):
    return test_info[2]

@action
def test_verification(test_info):
    verifier = test_info[3]
    if verifier is None:
        pytest.skip("Test case does not have special verification needed")

    return verifier

@action
def test_verification_result(default_condor, test_case, test_dag_handle, test_verification):
    with ChangeDir(test_case):
        return test_verification[0](default_condor, test_case, test_dag_handle)

@action
def test_verification_expected(test_verification):
    return test_verification[1]

#=========================================================================================
class TestDagAllNodes():
    def test_dag_exited_correctly(self, test_case, test_expected_dag_ec):
        with ChangeDir(test_case):
            with open(f"{DAG_FILENAME}.metrics", "r") as f:
                metrics = json.load(f)
                assert metrics["exitcode"] == test_expected_dag_ec

    def test_specific_verification(self, test_verification_result, test_verification_expected):
        assert test_verification_expected == test_verification_result

