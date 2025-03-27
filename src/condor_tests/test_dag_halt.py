#!/usr/bin/env pytest

# test_dag_halt.py
#
# This tests DAGMan halt DAGMan stops appropriately when
# halted (or resumed). This also verifies that the halt/resume
# interface through the python api DAGMan class and the
# htcondor cli dag noun's halt/resume verbs function correctly.
#
# Note: Most of the complexity is between the test waiting for
#       specific states and the logic in Node A's script to do
#       an action (resume DAG, nothing, hold/release DAG).

from ornithology import *
import htcondor2
import os
import json
import enum
from shutil import rmtree
from time import time as now

#------------------------------------------------------------------
# Enumeration of actions the Node A job can execute
class Actions(str, enum.Enum):
    NOTHING = "nothing"
    HOLD_N_RELEASE = "hold_and_release"
    RESUME_PY = "resume_py"
    RESUME_TOOL = "resume_tool"

# Timeout for all sections that wait on something
TIMEOUT = 30

# Various files for debugging and timing
CONTINUE_FILE = "dag.continue"
POLL_FILE = "job.running"
ERROR_FILE = "job.err"

# Test specific file that modifies the DAG appropriately
DAG_TEST_DETAILS = "test.details"

# Base DAG file to work with
DAG_FILENAME = "test.dag"
DAG_FILE = """
SUBMIT-DESCRIPTION touch @=desc
    executable = {touch_script}
    arguments  = "$(dag_node_name) job"
    universe   = local
    queue
@desc

JOB A @=NodeA
    executable = {act_on_dag_script}
    arguments  = "$(dag_node_name) $(ACTION) $(DAGManJobId)"
    universe   = local
    getenv     = true
    error      = {error_file}

    ACTION = {default_action}

    queue
@NodeA

JOB B touch

PARENT A CHILD B

SCRIPT PRE A {touch_script} $NODE pre
SCRIPT POST A {touch_script} $NODE post

SCRIPT PRE B {touch_script} $NODE pre
SCRIPT POST B {touch_script} $NODE post

INCLUDE {test_details}
"""

#==================================================================
# Send halt via python api
def python_halt(condor, ID: int) -> bool:
    success = False
    with condor.use_config():
        dm = htcondor2.DAGMan(ID)
        success, msg = dm.halt("Testing python api DAGMan.halt()")
        print(f"Halt {ID} via python api: {msg}")
    return success

#------------------------------------------------------------------
# Send a pause command via python api
def python_pause(condor, ID: int) -> bool:
    success = False
    with condor.use_config():
        dm = htcondor2.DAGMan(ID)
        success, msg = dm.halt("Testing python api DAGMan.halt() pauses DAG", True)
        print(f"Paused {ID} via python api: {msg}")
        if "paused" not in msg.lower():
            success = False
    return success

#------------------------------------------------------------------
# Send halt command via htcondor noun/verb cli
def tool_halt(condor, id: int) -> bool:
    p = condor.run_command(["htcondor", "dag", "halt", "--reason", "Testing htcondor dag halt", f"{id}"])
    output = p.stdout + p.stderr
    for line in output.split("\n"):
        if f"Halted DAG {id}" in line:
            return True
    return False

#------------------------------------------------------------------
# Class to hold specific test details:
#    - func: Python function to halt send halt command
#    - code: Expected exit code
#    - status: Expected final DAG status
#    - files: List of files (produced by node parts) that we expect to exist
class TestDetails:
    __test__ = False
    def __init__(self, func, code, status, files):
        self.func = func
        assert isinstance(code, int)
        self.code = code
        assert isinstance(status, int)
        self.status = status
        assert isinstance(files, list)
        self.files = files

#==================================================================
# Test cases to execute:
#     NAME : {
#       "inlcude": Test specific DAG file commands to add
#       "details": Test details for execution and verification
#     }
TEST_CASES = {
    "PY_HALT": {
        "details": TestDetails(python_halt, 1, 6, ["A.pre", "A.job", "A.post"]),
    },
    "PY_RESUME": {
        "include": f'VARS A APPEND ACTION="{Actions.RESUME_PY}"',
        "details": TestDetails(python_halt, 0, 0, ["A.pre", "A.job", "A.post", "B.pre", "B.job", "B.post"]),
    },
    "TOOL_HALT": {
        "details": TestDetails(tool_halt, 1, 6, ["A.pre", "A.job", "A.post"]),
    },
    "TOOL_RESUME": {
        "include": f'VARS A APPEND ACTION="{Actions.RESUME_TOOL}"',
        "details": TestDetails(tool_halt, 0, 0, ["A.pre", "A.job", "A.post", "B.pre", "B.job", "B.post"]),
    },
    # Special: Make sure DAGMan runs Final node once halted
    "HALT_RUN_FINAL": {
        "include":
"""
# FINAL Node should run when DAG is halted
# and finished with current work
FINAL F touch

SCRIPT PRE F {touch_script} $NODE pre
SCRIPT POST F {touch_script} $NODE post
""",
        "details": TestDetails(tool_halt, 0, 6, ["A.pre", "A.job", "A.post", "F.pre", "F.job", "F.post"])
    },
    # Special: Verify halt status last beyond crash (or DAGMan being held)
    "HALT_SURVIVE_RECOVERY": {
        "include": f'VARS A APPEND ACTION="{Actions.HOLD_N_RELEASE}"',
        "details": TestDetails(python_halt, 1, 6, ["A.pre", "A.job", "A.post"]),
    },
    # Special: Verify DAGMans ability to pause all progress
    "PAUSE_DAG": {
        "include": f'VARS A APPEND ACTION="{Actions.RESUME_PY}"',
        "details": TestDetails(python_pause, 0, 0, ["A.pre", "A.job", "A.post", "B.pre", "B.job", "B.post"])
    }
}

#==================================================================
# Script to touch a file: touch.py filename file_extension
@action
def write_touch_script(test_dir):
    script = test_dir / "touch.py"
    with open(script, "w") as f:
        f.write(r"""#!/usr/bin/env python3
import sys

if len(sys.argv) != 3:
    sys.exit(1)

fname = f"{sys.argv[1]}.{sys.argv[2]}"
with open(fname, "w") as f:
    pass

sys.exit(0)
""")
    os.chmod(script, 0o755)
    return script

#------------------------------------------------------------------
# Complex script to do some action(s) to parent DAGMan job once
# the test has informed us that the DAG is halted
# act_on_dag.py node_name action dag_id
@action
def write_action_script(test_dir):
    script = test_dir / "act_on_dag.py"
    with open(script, "w") as f:
        f.write(f"""#!/usr/bin/env python3
import sys
import htcondor2
import os
import subprocess
from time import time as now

def error(msg: str):
    sys.stderr.write(f"{{msg}}\\n")

if len(sys.argv) != 4:
    error(f"Invalid number of args ({{len(sys.argv)}}): Expected 4")
    sys.exit(1)

try:
    NODE = sys.argv[1]
    ACTION = sys.argv[2].lower()
    DAG_ID = int(sys.argv[3])
except Exception as e:
    error(f"Failed to process args: {{e}}")
    sys.exit(2)

# Inform test that node A job is executing
with open("{POLL_FILE}", "w") as f:
    pass

# Wait for test to inform that DAG has been sent halt command
start = now()
while True:
    if now() - start >= {TIMEOUT}:
        error("Timed out waiting on test infrastructure signal file '{CONTINUE_FILE}'")
        sys.exit(3)
    if os.path.exists("{CONTINUE_FILE}"):
        break

# Do specified action
if ACTION == "{Actions.HOLD_N_RELEASE}":
    # Hold parent DAG job
    schedd = htcondor2.Schedd()
    result = schedd.act(htcondor2.JobAction.Hold, f"{{DAG_ID}}.0")

    if result["TotalSuccess"] != 1:
        error(f"Failed to put DAG {{DAG_ID}} on hold")
        sys.exit(10)

    # Wait for DAGMan jobs hold event
    with htcondor2.JobEventLog("{DAG_FILENAME}.dagman.log") as JEL:
        start = now()
        held = False
        while True:
            if now() - start >= {TIMEOUT}:
                error("Timed out waiting for hold event after {TIMEOUT} seconds.")
                sys.exit(11)
            for event in JEL.events(stop_after=1):
                assert event.cluster == DAG_ID
                if event.type == htcondor2.JobEventType.JOB_HELD:
                    held = True
                    break
            if held:
                break

    # Release the DAG
    result2 = schedd.act(htcondor2.JobAction.Release, f"{{DAG_ID}}.0")
    if result2["TotalSuccess"] != 1:
        error(f"Failed to release DAG {{DAG_ID}}")
        sys.exit(12)

elif ACTION == "{Actions.RESUME_PY}":
    # Send resume command to DAG via python api
    dm = htcondor2.DAGMan(DAG_ID)
    success, msg = dm.resume()
    if not success:
        error(f"Failed to send resume command to DAG {{DAG_ID}}: {{msg}}")
        sys.exit(20)

elif ACTION == "{Actions.RESUME_TOOL}":
    # Send resume command to DAG via htcondor noun/verb cli
    p = subprocess.run(
        ["htcondor", "dag", "resume", f"{{DAG_ID}}"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=5,
    )

    success = False
    output = p.stdout.decode() + p.stderr.decode()
    for line in output.split("\\n"):
        if f"Resumed DAG {{DAG_ID}}" in line:
            success = True
            break

    if not success:
        error(f"Failed to resume DAG {{DAG_ID}} from htc-cli:\\n\\n{{output}}")
        sys.exit(30)

elif ACTION == "{Actions.NOTHING}":
    # Do nothing
    pass

else:
    # Unknown action
    error(f"Unknown action provided: {{ACTION}}")
    sys.exit(4)

# Touch our job file
done_file = NODE + ".job"
with open(done_file, "w") as f:
    pass

sys.exit(0)
""")
    os.chmod(script, 0o755)
    return script

#------------------------------------------------------------------
@action
def run_dags(default_condor, test_dir, write_touch_script, write_action_script):
    TESTS = dict()

    DAG_MODIFIERS = {
        "touch_script": write_touch_script,
        "act_on_dag_script": write_action_script,
        "test_details": DAG_TEST_DETAILS,
        "error_file": ERROR_FILE,
        "default_action": Actions.NOTHING,
    }

    for test, info in TEST_CASES.items():
        # Create and change into sub-directory named after test case
        os.chdir(str(test_dir))
        exe_dir = test_dir / test
        if exe_dir.exists():
            rmtree(exe_dir)
        os.mkdir(str(exe_dir))
        os.chdir(str(exe_dir))

        # Write base DAG file
        with open(DAG_FILENAME, "w") as f:
            f.write(DAG_FILE.format(**DAG_MODIFIERS))

        # Write test specific DAG include file (or empty dummy)
        with open(DAG_TEST_DETAILS, "w") as f:
            f.write(
                info.get(
                    "include",
                    "# Test is using default information\n"
                ).format(**DAG_MODIFIERS)
            )

        # Submit DAG
        DAG = htcondor2.Submit.from_dag(DAG_FILENAME)
        HANDLE = default_condor.submit(DAG)
        TESTS[test] = (HANDLE, info["details"])

    # Change back to root test dir
    os.chdir(str(test_dir))

    yield TESTS

#------------------------------------------------------------------
@action(params={name: name for name in TEST_CASES})
def test_info(request, run_dags, test_dir):
    return (request.param, run_dags[request.param][0], run_dags[request.param][1])

@action
def test_name(test_info):
    return test_info[0]

@action
def test_handle(test_info):
    return test_info[1]

@action
def test_details(test_info):
    return test_info[2]

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
class TestDAGManHalt:
    def test_check_run_actions_and_wait(self, test_name, test_handle, test_details, default_condor):
        with ChangeDir(test_name) as dag_dir:
            # Wait for node A job to inform us that is executing
            start = now()
            while True:
                assert now() - start < TIMEOUT
                if os.path.exists(POLL_FILE):
                    break
            # Run test specific hatl command
            assert test_details.func(default_condor, test_handle.clusterid)
            # Inform node A job that we have sent halt command
            with open(CONTINUE_FILE, "w") as f:
                pass
            # Wait for DAG job to complete
            assert test_handle.wait(condition=ClusterState.all_complete, timeout=TIMEOUT)


    def test_check_final_status(self, test_name, test_details):
        with ChangeDir(test_name) as dag_dir:
            # Check expected exit code and DAG status to recorded metrics
            with open(f"{DAG_FILENAME}.metrics", "r") as f:
                metrics = json.load(f)
                assert metrics["exitcode"] == test_details.code
                assert metrics["DagStatus"] == test_details.status


    def test_check_expected_files(self, test_name, test_details):
        # Special files we expect to exist but are not part of test verification
        IGNORE = [
            CONTINUE_FILE,
            POLL_FILE,
            ERROR_FILE,
            DAG_TEST_DETAILS,
        ]
        expected = test_details.files
        success = True

        with ChangeDir(test_name) as dag_dir:
            # Iterate over test directory
            for path in os.listdir():
                if path in IGNORE or path.startswith(DAG_FILENAME):
                    # Skip DAG files and explict ignore files
                    continue
                elif path in expected:
                    expected.remove(path)
                else:
                    print(f"Unexpected file discovered: {path}")
                    success = False

            # Verify we found all expected files
            if len(expected) > 0:
                success = False
                for f in expected:
                    print(f"Missing expected file: {f}")

        assert success
