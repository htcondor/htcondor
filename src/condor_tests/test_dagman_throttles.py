#!/usr/bin/env pytest

# This test checks the following:
#     1. Ability to set DAGMan throttles via Python API
#     2. Ability to set DAGMan throttles via htcondor cli (uses python API)
#     3. Admin throttle limiting
#          A. Check that user defined throttles get limited by admin set values
#          B. Ensure user cannot bypass admin limit via configuration variables
#             set in the DAGMan config file
#          C. Verify admin throttle limit disable configuration option functions
#          D. Verify admin only options set via a root config include are honored
#          E. Verify DAGMan exits with an error when admin only options are set
#             via _CONDOR_* environment variables or the user config file
#             (USER_CONFIG_FILE) including any files it includes

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

# Administrator set throttle limits (ceilings/floor)
ADMIN_LIMITS = {
    "DAGMAN_USER_LOG_SCAN_INTERVAL_FLOOR": 10,
    "DAGMAN_MAX_JOBS_IDLE_CEILING": 10,
    "DAGMAN_MAX_JOBS_SUBMITTED_CEILING": 10,
    "DAGMAN_MAX_PRE_SCRIPTS_CEILING": 10,
    "DAGMAN_MAX_HOLD_SCRIPTS_CEILING": 10,
    "DAGMAN_MAX_POST_SCRIPTS_CEILING": 10,
    "DAGMAN_MAX_SUBMITS_PER_INTERVAL_CEILING": 10,
}

# User level configuration attempting to raise/disable the admin limits in ADMIN_LIMITS
USER_RAISE_LIMITS = {
    "DAGMAN_USER_LOG_SCAN_INTERVAL": 1,
    "DAGMAN_MAX_JOBS_IDLE": 500,
    "DAGMAN_MAX_JOBS_SUBMITTED": 500,
    "DAGMAN_MAX_PRE_SCRIPTS": 500,
    "DAGMAN_MAX_HOLD_SCRIPTS": 500,
    "DAGMAN_MAX_POST_SCRIPTS": 500,
    "DAGMAN_MAX_SUBMITS_PER_INTERVAL": 500,
    "DAGMAN_USER_LOG_SCAN_INTERVAL_FLOOR": 1,
    "DAGMAN_MAX_JOBS_IDLE_CEILING": 500,
    "DAGMAN_MAX_JOBS_SUBMITTED_CEILING": 500,
    "DAGMAN_MAX_PRE_SCRIPTS_CEILING": 500,
    "DAGMAN_MAX_HOLD_SCRIPTS_CEILING": 500,
    "DAGMAN_MAX_POST_SCRIPTS_CEILING": 500,
    "DAGMAN_MAX_SUBMITS_PER_INTERVAL_CEILING": 500,
}

USER_DISABLE_LIMITS = {
    **USER_RAISE_LIMITS,
    "DAGMAN_DISABLE_ADMIN_THROTTLE_LIMITING": True,
}

def to_config_text(config: Dict[str, Any]) -> str:
    return "\n".join(f"{key} = {value}" for key, value in config.items()) + "\n"

def write_admin_limits(test_dir: Path) -> Path:
    """Write the admin limits to a file to be included by the root config"""
    admin_limits = test_dir / "admin_limits.config"
    admin_limits.write_text(to_config_text(ADMIN_LIMITS))
    return admin_limits

# Throttles to set when a user attempts to bypass admin limits
BYPASS_THROTTLES = {"max_pre": 5}

# Expected throttles when user attempts to bypass admin limits
BYPASS_EXPECTED = classad2.ClassAd({
    "DAGMan_MaxJobs": 10,
    "DAGMan_MaxIdle": 10,
    "DAGMan_MaxPreScripts": 5,
    "DAGMan_MaxHoldScripts": 10,
    "DAGMan_MaxPostScripts": 10,
    "DAGMan_MaxSubmitsPerInterval": 10,
})

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
    def __init__(self, exe: Callable[[Condor, int, Any], classad2.ClassAd], throttles: Any, expected: classad2.ClassAd, conf: Config = Config.DEFAULT, extra: str = "",
                 dag_config: Optional[Dict[str, Any]] = None, scan_limited: bool = False) -> None:
        self.execute = exe
        self.throttles = throttles
        self.expected = expected
        self.conf = conf
        self.extra = extra
        self.dag_config = dag_config # DAGMan config file (CONFIG command) contents
        self.scan_limited = scan_limited # Expect admin minimum log scan interval to be enforced


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
    # Check setting invalid throttles (outside of default admin ceilings: 2x the default value) with default configuration
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
            "DAGMan_MaxIdle": 2000,
            "DAGMan_MaxPreScripts": 40,
            "DAGMan_MaxHoldScripts": 40,
            "DAGMan_MaxPostScripts": 40,
            "DAGMan_MaxSubmitsPerInterval": 200,
        })
    ),
    # Check setting infinite for throttles with default configuration (limited to default admin ceilings)
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
            "DAGMan_MaxIdle": 2000,
            "DAGMan_MaxPreScripts": 40,
            "DAGMan_MaxHoldScripts": 40,
            "DAGMan_MaxPostScripts": 40,
            "DAGMan_MaxSubmitsPerInterval": 200,
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
    # Verify user can not raise admin limits via DAGMan config file
    "DAG_CONFIG_RAISE_LIMITS": DagTestCase(
        throttle_via_python,
        BYPASS_THROTTLES,
        BYPASS_EXPECTED,
        Config.LIMITED,
        dag_config=USER_RAISE_LIMITS,
        scan_limited=True,
    ),
    # Verify user can not disable admin limits via DAGMan config file
    "DAG_CONFIG_DISABLE_LIMITS": DagTestCase(
        throttle_via_python,
        BYPASS_THROTTLES,
        BYPASS_EXPECTED,
        Config.LIMITED,
        dag_config=USER_DISABLE_LIMITS,
        scan_limited=True,
    ),
}

#-----------------------------------------------------------------------------------------
@action
def limited_condor(test_dir):
    """Condor configured with low sweeping set of throttle limits (admin limits set via root config include)"""
    local_dir = test_dir / "limited_condor"

    with Condor(
        local_dir=local_dir,
        config=SHARED_CONFIG,
        raw_config=f"include : {write_admin_limits(test_dir)}\n",
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
def select_condor(conf: Config, default_condor, limited_condor, limitless_condor) -> Condor:
    if conf == Config.LIMITED:
        return limited_condor
    elif conf == Config.LIMITLESS:
        return limitless_condor
    return default_condor

#-----------------------------------------------------------------------------------------
@action
def run_dags(default_condor, limited_condor, limitless_condor, test_dir, path_to_sleep):
    TESTS = dict()

    for TEST, DETAILS in TEST_CASES.items():
        case_dir = test_dir / TEST
        if case_dir.exists():
            rmtree(case_dir)

        os.mkdir(case_dir)

        extra = DETAILS.extra
        if DETAILS.dag_config is not None:
            DAG_CONFIG = case_dir / "dagman.config"
            DAG_CONFIG.write_text(to_config_text(DETAILS.dag_config))
            extra += f"\nCONFIG {DAG_CONFIG}\n"

        DAG_FILE = case_dir / DAG_FILENAME
        with open(DAG_FILE, "w") as f:
            f.write(f"""
JOB LONG @=desc
    executable = {path_to_sleep}
    arguments  = 1000000
@desc

{extra}
""")

        # Select which condor to submit to (different configurations)
        condor = select_condor(DETAILS.conf, default_condor, limited_condor, limitless_condor)

        with ChangeDir(TEST):
            DAG = htcondor2.Submit.from_dag(str(DAG_FILE))
            HANDLE = condor.submit(DAG)
            TESTS[TEST] = (HANDLE, DETAILS.conf, DETAILS.execute, DETAILS.throttles, DETAILS.expected, DETAILS.scan_limited)

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
        TEST[5],        # Test case expects admin scan interval enforced
    )

@action
def test_case(test_info):
    return test_info[0]

@action
def test_handle(test_info):
    return test_info[1]

@action
def test_condor(test_info, default_condor, limited_condor, limitless_condor):
    return select_condor(test_info[2], default_condor, limited_condor, limitless_condor)

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
def test_scan_limited(test_info):
    return test_info[6]

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

    def test_admin_scan_interval_enforced(self, test_dir, test_case, throttle_dag, test_scan_limited):
        if not test_scan_limited:
            pytest.skip("Test case does not attempt to bypass admin minimum log scan interval")

        dagman_out = test_dir / test_case / f"{DAG_FILENAME}.dagman.out"
        contents = dagman_out.read_text()
        assert "Specified scan interval 1 is less than administrator limit" in contents
        assert "DAGMAN_USER_LOG_SCAN_INTERVAL setting: 10" in contents


#=========================================================================================
# Admin only option source enforcement: DAGMan should exit with an error if any
# admin only option is set by something other than the root config
ADMIN_ONLY_ERROR = "is an administrator only configuration option but was set by"

class AdminOnlyCase():
    def __init__(self, user_config: bool, expected: Dict[str, str], env: Optional[Dict[str, str]] = None) -> None:
        self.user_config = user_config # Submit to condor with USER_CONFIG_FILE set
        self.expected = expected # Map of admin only option to expected (partial) location in error message
        self.env = env if env is not None else dict() # Environment to submit DAG with


ADMIN_ONLY_CASES = {
    # Admin ceiling set via _CONDOR_* environment variable
    "ENV_CEILING": AdminOnlyCase(
        False,
        {"DAGMAN_MAX_JOBS_IDLE_CEILING": "<Environment>"},
        env={"_CONDOR_DAGMAN_MAX_JOBS_IDLE_CEILING": "500"},
    ),
    # Admin limiting disabled and floor lowered via _CONDOR_* environment variables
    "ENV_DISABLE": AdminOnlyCase(
        False,
        {
            "DAGMAN_DISABLE_ADMIN_THROTTLE_LIMITING": "<Environment>",
            "DAGMAN_USER_LOG_SCAN_INTERVAL_FLOOR": "<Environment>",
        },
        env={
            "_CONDOR_DAGMAN_DISABLE_ADMIN_THROTTLE_LIMITING": "True",
            "_CONDOR_DAGMAN_USER_LOG_SCAN_INTERVAL_FLOOR": "1",
        },
    ),
    # Admin ceilings set directly in the user config and via a file the user config includes
    "USER_CONFIG": AdminOnlyCase(
        True,
        {
            "DAGMAN_MAX_POST_SCRIPTS_CEILING": "user.config",
            "DAGMAN_MAX_JOBS_IDLE_CEILING": "user_include.config",
        },
    ),
}

#-----------------------------------------------------------------------------------------
@action
def user_config_condor(test_dir):
    """Condor configured with admin limits via root config include plus a user config that attempts to override them"""
    local_dir = test_dir / "user_config_condor"

    user_include = test_dir / "user_include.config"
    user_include.write_text(to_config_text({"DAGMAN_MAX_JOBS_IDLE_CEILING": 500}))

    # Note: avoid file names matching (case insensitive) a test case directory name
    user_config = test_dir / "user.config"
    user_config.write_text(to_config_text({"DAGMAN_MAX_POST_SCRIPTS_CEILING": 500}) + f"include : {user_include}\n")

    with Condor(
        local_dir=local_dir,
        config={**SHARED_CONFIG, "USER_CONFIG_FILE": str(user_config)},
        raw_config=f"include : {write_admin_limits(test_dir)}\n",
    ) as condor:
        yield condor

#-----------------------------------------------------------------------------------------
@action
def run_admin_only_dags(limited_condor, user_config_condor, test_dir, path_to_sleep):
    DAGS = dict()

    for TEST, DETAILS in ADMIN_ONLY_CASES.items():
        case_dir = test_dir / TEST
        if case_dir.exists():
            rmtree(case_dir)

        os.mkdir(case_dir)

        DAG_FILE = case_dir / DAG_FILENAME
        DAG_FILE.write_text(f"""
JOB LONG @=desc
    executable = {path_to_sleep}
    arguments  = 1
@desc
""")

        condor = user_config_condor if DETAILS.user_config else limited_condor

        # DAGMan job inherits _CONDOR_* environment variables at submit time
        with ChangeDir(TEST), SetEnv(DETAILS.env):
            DAG = htcondor2.Submit.from_dag(str(DAG_FILE))
            DAGS[TEST] = (condor.submit(DAG), DETAILS.expected)

    yield DAGS

@action(params={name: name for name in ADMIN_ONLY_CASES})
def admin_only_case(request, run_admin_only_dags):
    return (request.param, *run_admin_only_dags[request.param])

@action
def admin_only_dagman_out(test_dir, admin_only_case):
    name, handle, _ = admin_only_case
    assert handle.wait(condition=ClusterState.all_complete, timeout=TIMEOUT)
    return (test_dir / name / f"{DAG_FILENAME}.dagman.out").read_text()


class TestAdminOnlyConfig():
    def test_dagman_exits_with_error(self, admin_only_dagman_out):
        assert "EXITING WITH STATUS 1" in admin_only_dagman_out

    def test_admin_only_options_reported(self, admin_only_case, admin_only_dagman_out):
        _, _, expected = admin_only_case
        for knob, location in expected.items():
            lines = [line for line in admin_only_dagman_out.splitlines() if f"{knob} {ADMIN_ONLY_ERROR}" in line]
            assert len(lines) == 1, f"Expected error for {knob} not found"
            assert location in lines[0]

    def test_dag_not_run(self, test_dir, admin_only_case, admin_only_dagman_out):
        name, _, _ = admin_only_case
        # DAGMan exits before parsing the DAG so no node jobs are ever submitted
        assert not (test_dir / name / f"{DAG_FILENAME}.nodes.log").exists()
