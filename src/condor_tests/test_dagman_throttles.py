#!/usr/bin/env pytest

# This test checks the following:
#     1. Ability to set DAGMan throttles via Python API
#     2. Ability to set DAGMan throttles via htcondor cli (uses python API)
#     3. Admin throttle limiting
#          A. Check that user defined throttles get limited by admin set values
#          B. Ensure user cannot bypass admin limit via configuration variables
#          C. Verify admin throttle limit disable configuration option functions

from ornithology import *
import htcondor2
import classad2
import pytest
import os
import sys
import enum

from time import sleep, time
from pathlib import Path
from shutil import rmtree
from typing import Callable, Any, List, Optional, Dict

#-----------------------------------------------------------------------------------------
DAG_FILENAME = "test.dag"
TIMEOUT = 120
SHARED_CONFIG = {
    "DAGMAN_USER_LOG_SCAN_INTERVAL": 10,
    "DAGMAN_MAX_JOBS_IDLE": 10,
    "DAGMAN_MAX_JOBS_SUBMITTED": 10,
    "DAGMAN_MAX_PRE_SCRIPTS": 10,
    "DAGMAN_MAX_HOLD_SCRIPTS": 10,
    "DAGMAN_MAX_POST_SCRIPTS": 10,
    "DAGMAN_MAX_SUBMITS_PER_INTERVAL": 10,
}

#-----------------------------------------------------------------------------------------
class SetThrottleError(Exception):
    def __init__(self, message="Failed to set DAGMan throttles") -> None:
        self.message = message
        super().__init__(self.message)

#-----------------------------------------------------------------------------------------
def throttle_via_python(condor: Condor, dagid: int, throttles: Dict[str, int]) -> classad2.ClassAd:
    with condor.use_config():
        dag = htcondor2.DAGMan(dagid)
        result = dag.throttle(**throttles)
        if isinstance(result, str):
            raise SetThrottleError(result)

    return result

#-----------------------------------------------------------------------------------------
def throttle_via_htc_cli(condor: Condor, dagid: int, args: List[str]) -> classad2.ClassAd:
    command = ["htcondor", "dag", "throttle", f"{dagid}", *args]
    p = condor.run_command(command)
    if p.returncode != 0:
        raise SetThrottleError(f"Failed to execute htcondor dag throttle: {p.stdout} {p.stderr}")

    result = classad2.ClassAd()

    KEY_TO_ATTR = {
        "idle": "DAGMan_MaxIdle",
        "pre": "DAGMan_MaxPreScripts",
        "hold": "DAGMan_MaxHoldScripts",
        "post": "DAGMan_MaxPostScripts",
        "submitted": "DAGMan_MaxJobs",
        "submits": "DAGMan_MaxSubmitsPerInterval",
    }

    try:
        for line in p.stdout.split("\n"):
            line = line.strip().lower()
            if line.startswith("maximum"):
                details, value = line.split(":")
                _, key, _ = details.split(" ", 2)
                result[KEY_TO_ATTR[key]] = int(value.strip())
    except Exception as e:
        raise SetThrottleError(f"Failed to process htcondor dag throttle output: {e}")

    return result

#-----------------------------------------------------------------------------------------
class Config(enum.Enum):
    """Enum to discern which configured condor to run DAGs"""
    DEFAULT = 0
    LIMITED = 1
    LIMITLESS = 2


class DagTestCase():
    def __init__(self, exe: Callable[[Condor, int, Any], classad2.ClassAd], throttles: Any, expected: classad2.ClassAd, conf: Config = Config.DEFAULT, extra: str = "") -> None:
        self.execute = exe
        self.throttles = throttles
        self.expected = expected
        self.conf = conf
        self.extra = extra


TEST_CASES = {
    # Check setting valid throttles with default configuration
    "SET_VALID": DagTestCase(
        throttle_via_python,
        {
            "max_nodes": 10,
            "max_idle": 10,
            "max_pre": 10,
            "max_hold": 10,
            "max_post": 10,
            "max_submits": 10,
        },
        classad2.ClassAd({
            "DAGMan_MaxJobs": 10,
            "DAGMan_MaxIdle": 10,
            "DAGMan_MaxPreScripts": 10,
            "DAGMan_MaxHoldScripts": 10,
            "DAGMan_MaxPostScripts": 10,
            "DAGMan_MaxSubmitsPerInterval": 10,
        })
    ),
    # Check setting invalid throttles (outside of admin limit) with default configuration
    "SET_INVALID": DagTestCase(
        throttle_via_python,
        {
            "max_nodes": 99999, # Note: by default this is valid
            "max_idle": 99999,
            "max_pre": 99999,
            "max_hold": 99999,
            "max_post": 99999,
            "max_submits": 99999,
        },
        classad2.ClassAd({
            "DAGMan_MaxJobs": 99999,
            "DAGMan_MaxIdle": 1000,
            "DAGMan_MaxPreScripts": 20,
            "DAGMan_MaxHoldScripts": 20,
            "DAGMan_MaxPostScripts": 20,
            "DAGMan_MaxSubmitsPerInterval": 100,
        })
    ),
    # Check setting infinite for throttles with default configuration
    "SET_INFINITE": DagTestCase(
        throttle_via_python,
        {
            "max_nodes": 0,
            "max_idle": 0,
            "max_pre": 0,
            "max_hold": 0,
            "max_post": 0,
            "max_submits": 0,
        },
        classad2.ClassAd({
            "DAGMan_MaxJobs": 0,
            "DAGMan_MaxIdle": 1000,
            "DAGMan_MaxPreScripts": 20,
            "DAGMan_MaxHoldScripts": 20,
            "DAGMan_MaxPostScripts": 20,
            "DAGMan_MaxSubmitsPerInterval": 100,
        })
    ),
    # Check setting valid throttles with lower admin limit configured
    "LIMITED_SET_VALID": DagTestCase(
        throttle_via_python,
        {
            "max_nodes": 1,
            "max_idle": 2,
            "max_pre": 3,
            "max_hold": 4,
            "max_post": 5,
            "max_submits": 6,
        },
        classad2.ClassAd({
            "DAGMan_MaxJobs": 1,
            "DAGMan_MaxIdle": 2,
            "DAGMan_MaxPreScripts": 3,
            "DAGMan_MaxHoldScripts": 4,
            "DAGMan_MaxPostScripts": 5,
            "DAGMan_MaxSubmitsPerInterval": 6,
        }),
        Config.LIMITED
    ),
    # Check setting invalid throttles with lower admin limit configured
    "LIMITED_SET_INVALID": DagTestCase(
        throttle_via_python,
        {
            "max_nodes": 100,
            "max_idle": 200,
            "max_pre": 300,
            "max_hold": 400,
            "max_post": 500,
            "max_submits": 600,
        },
        classad2.ClassAd({
            "DAGMan_MaxJobs": 10,
            "DAGMan_MaxIdle": 10,
            "DAGMan_MaxPreScripts": 10,
            "DAGMan_MaxHoldScripts": 10,
            "DAGMan_MaxPostScripts": 10,
            "DAGMan_MaxSubmitsPerInterval": 10,
        }),
        Config.LIMITED
    ),
    # Check setting infinite throttles with lower admin limit configured
    "LIMITED_SET_INFINITE": DagTestCase(
        throttle_via_python,
        {
            "max_nodes": 0,
            "max_idle": 0,
            "max_pre": 0,
            "max_hold": 0,
            "max_post": 0,
            "max_submits": 0,
        },
        classad2.ClassAd({
            "DAGMan_MaxJobs": 10,
            "DAGMan_MaxIdle": 10,
            "DAGMan_MaxPreScripts": 10,
            "DAGMan_MaxHoldScripts": 10,
            "DAGMan_MaxPostScripts": 10,
            "DAGMan_MaxSubmitsPerInterval": 10,
        }),
        Config.LIMITED
    ),
    # Check setting throttles above defaults without admin limiting configured
    "LIMITLESS": DagTestCase(
        throttle_via_python,
        {
            "max_nodes": 100,
            "max_idle": 200,
            "max_pre": 300,
            "max_hold": 400,
            "max_post": 500,
            "max_submits": 600,
        },
        classad2.ClassAd({
            "DAGMan_MaxJobs": 100,
            "DAGMan_MaxIdle": 200,
            "DAGMan_MaxPreScripts": 300,
            "DAGMan_MaxHoldScripts": 400,
            "DAGMan_MaxPostScripts": 500,
            "DAGMan_MaxSubmitsPerInterval": 600,
        }),
        Config.LIMITLESS
    ),
    # Check setting infinite throttles without admin limiting configured
    "LIMITLESS_INFINITE": DagTestCase(
        throttle_via_python,
        {
            "max_nodes": 0,
            "max_idle": 0,
            "max_pre": 0,
            "max_hold": 0,
            "max_post": 0,
            "max_submits": 0,
        },
        classad2.ClassAd({
            "DAGMan_MaxJobs": 0,
            "DAGMan_MaxIdle": 0,
            "DAGMan_MaxPreScripts": 0,
            "DAGMan_MaxHoldScripts": 0,
            "DAGMan_MaxPostScripts": 0,
            "DAGMan_MaxSubmitsPerInterval": 0,
        }),
        Config.LIMITLESS
    ),
    # Verify htcondor dag throttle sets limits correctly
    "HTC_CLI": DagTestCase(
        throttle_via_htc_cli,
        ["--nodes", "10", "--idle", "10", "--pre", "10", "--post", "10", "--hold", "10", "--submissions", "10"],
        classad2.ClassAd({
            "DAGMan_MaxJobs": 10,
            "DAGMan_MaxIdle": 10,
            "DAGMan_MaxPreScripts": 10,
            "DAGMan_MaxHoldScripts": 10,
            "DAGMan_MaxPostScripts": 10,
            "DAGMan_MaxSubmitsPerInterval": 10,
        }),
    ),
    # Verify the ability to set specific throttles without effecting others
    "SET_SPECIFIC": DagTestCase(
        throttle_via_python,
        {
            "max_pre": 5,
            "max_post": 10,
        },
        classad2.ClassAd({
            "DAGMan_MaxJobs": 0,
            "DAGMan_MaxIdle": 1000,
            "DAGMan_MaxPreScripts": 5,
            "DAGMan_MaxHoldScripts": 20,
            "DAGMan_MaxPostScripts": 10,
            "DAGMan_MaxSubmitsPerInterval": 100,
        }),
    ),
}

#-----------------------------------------------------------------------------------------
@action
def limited_condor(test_dir):
    """Condor configured with low sweeping set of throttle limits"""
    local_dir = test_dir / "limited_condor"

    with Condor(
        local_dir=local_dir,
        config=SHARED_CONFIG,
    ) as condor:
        yield condor

#-----------------------------------------------------------------------------------------
@action
def limitless_condor(test_dir):
    """Condor configured with low sweeping set of throttle values but no enforcing admin limit"""
    local_dir = test_dir / "limitless_condor"

    conf = SHARED_CONFIG.copy()
    conf["DAGMAN_DISABLE_ADMIN_THROTTLE_LIMITING"] = True

    with Condor(
        local_dir=local_dir,
        config=conf,
    ) as condor:
        yield condor

#-----------------------------------------------------------------------------------------
@action
def run_dags(default_condor, limited_condor, limitless_condor, test_dir, path_to_sleep):
    TESTS = dict()

    for TEST, DETAILS in TEST_CASES.items():
        case_dir = test_dir / TEST
        if case_dir.exists():
            rmtree(case_dir)

        os.mkdir(case_dir)

        DAG_FILE = case_dir / DAG_FILENAME
        with open(DAG_FILE, "w") as f:
            f.write(f"""
JOB LONG @=desc
    executable = {path_to_sleep}
    arguments  = 1000000
@desc

{DETAILS.extra}
""")

        # Select which condor to submit to (different configurations)
        condor = default_condor
        if DETAILS.conf == Config.LIMITED:
            condor = limited_condor
        elif DETAILS.conf == Config.LIMITLESS:
            condor = limitless_condor

        with ChangeDir(TEST):
            DAG = htcondor2.Submit.from_dag(str(DAG_FILE))
            HANDLE = condor.submit(DAG)
            TESTS[TEST] = (HANDLE, DETAILS.conf, DETAILS.execute, DETAILS.throttles, DETAILS.expected)

    yield TESTS


#-----------------------------------------------------------------------------------------
@action(params={name: name for name in TEST_CASES})
def test_info(request, run_dags):
    TEST = run_dags[request.param]

    return (
        request.param,  # Test case name
        TEST[0],        # Test case job handle
        TEST[1],        # Test case which condor instance
        TEST[2],        # Test case throttle function
        TEST[3],        # Test case throttle information
        TEST[4],        # Test case expected results
    )

@action
def test_case(test_info):
    return test_info[0]

@action
def test_handle(test_info):
    return test_info[1]

@action
def test_condor(test_info, default_condor, limited_condor, limitless_condor):
    condor = default_condor
    if test_info[2] == Config.LIMITED:
        condor = limited_condor
    elif test_info[2] == Config.LIMITLESS:
        condor = limitless_condor

    return condor

@action
def test_throttle_func(test_info):
    return test_info[3]

@action
def test_throttles(test_info):
    return test_info[4]

@action
def test_expected(test_info):
    return test_info[5]

@action
def throttle_dag(test_case, test_condor, test_handle, test_throttle_func, test_throttles):
    assert test_handle.wait(condition=ClusterState.all_running, timeout=TIMEOUT)

    # Log says DAG is running... wait until nodes.log exists (meaning job(s)
    # have been submitted) to ensure DAG is actually running (or else commands fail)
    with ChangeDir(test_case):
        check = Path(f"{DAG_FILENAME}.nodes.log")
        start = time()
        while time() - start <= TIMEOUT and not check.exists():
            sleep(1)

    try:
        result = test_throttle_func(test_condor, test_handle.clusterid, test_throttles)
    except SetThrottleError as e:
        pytest.fail(str(e))

    # We don't actually need to wait for DAG completion so just remove it
    # to assist cleanup
    test_handle.remove()

    return result


#=========================================================================================
class TestDagThrottling():
    def test_setting_dag_throttles(self, throttle_dag, test_expected):
        assert throttle_dag == test_expected
