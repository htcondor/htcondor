#!/usr/bin/env pytest

import pytest
from ornithology import *
import os
import json
from shutil import rmtree

#------------------------------------------------------------------
# Magical class to cd into specific dir and return to starting dir
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
def setup_use_dag_dir():
    SUBDIR = "subdir"
    os.mkdir(SUBDIR)

    with ChangeDir(SUBDIR):
        with open("test.dag", "w") as f:
            f.write("INCLUDE info.inc\n")

        with open("info.inc", "w") as f:
            f.write("JOB A desc\n")

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
def setup_complex_dag():
    with open("sub-splice.dag", "w") as f:
        f.write("JOB A desc\n")

    with open("splice.dag", "w") as f:
        f.write("""# Splice DAG
JOB A desc
JOB B desc

SPLICE DEEP sub-splice.dag

PARENT A CHILD DEEP
""")

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
def setup_validation_errors():
    with open("single-node.dag", "w") as f:
        f.write("JOB A desc\n")

    with open("recursive.dag", "w") as f:
        f.write("SPLICE UH-OH recursive.dag\n")

    with open("recursive.inc", "w") as f:
        f.write("INCLUDE recursive.inc\n")

    with open("splice-with-final.dag", "w") as f:
        f.write("FINAL F desc\n")

    SUBDIR = "subdir"

    os.mkdir(SUBDIR)

    with open("splice.dag", "w") as f:
        f.write(f"SPLICE S sub-splice.dag DIR {SUBDIR}\n")

    with ChangeDir(SUBDIR):
        with open("sub-splice.dag", "w") as f:
            f.write("SPLICE S sub-sub-splice.dag\n")

        with open("sub-sub-splice.dag", "w") as f:
            f.write("JOB\n")

#------------------------------------------------------------------
KEY_DAG = "DagFile"
KEY_SETUP = "SetupFunc"
KEY_EXIT = "ExitCode"
KEY_ERRORS = "Errors"
KEY_STATS = "Stats"
KEY_OPTIONS = "ExtraCmdOptions"
KEY_OVERRIDE = "OverrideRootDagFile"

TEST_CASES = {
    # Test Case: Check parser syntax errors (note full parser syntax errors checked in unit test)
    "PARSE_ERRORS": {
        KEY_DAG: """# DAG riddled with syntax errors
JOB all_nodes desc
FINAL FOO+BAR desc
RETRY
DNE blah blah blah
""",
        KEY_EXIT: 1,
        KEY_ERRORS: [
            {
                "Reason": "Node name is a reserved word",
                "SourceFile": "PARSE_ERRORS.dag",
                "SourceLine": 2,
                "DagCommand": "JOB",
            },
            {
                "Reason": "Node name contains illegal charater",
                "SourceFile": "PARSE_ERRORS.dag",
                "SourceLine": 3,
                "DagCommand": "FINAL",
            },
            {
                "Reason": "No node name specified",
                "SourceFile": "PARSE_ERRORS.dag",
                "SourceLine": 4,
                "DagCommand": "RETRY",
            },
            {
                "Reason": "'DNE' is not a valid DAG command",
                "SourceFile": "PARSE_ERRORS.dag",
                "SourceLine": 5,
            },
        ],
    },
    # Test Case: Check post syntax parse validation errors
    "VALIDATION_ERRORS": {
        KEY_DAG: """# Syntactically correct DAG with validation issues
FINAL F desc
FINAL F2 desc
PROVISIONER P desc
PROVISIONER P2 desc
SERVICE S desc

VARS ALL_NODES foo="bar"
DONE ALL_NODES

SPLICE NO_FILE dne.dag
SPLICE S1 single-node.dag
SPLICE S2 splice-with-final.dag
SPLICE S3 splice.dag
SPLICE RECURSIVE recursive.dag

INCLUDE dne.inc
INCLUDE recursive.inc

JOB A desc
JOB A desc
JOB S1 desc
SPLICE A single-node.dag

PARENT DNE CHILD A
PARENT A CHILD DNE
PARENT DNE CHILD DNE
PARENT F P S CHILD A

RETRY S1 3
PRIORITY DNE 5

CATEGORY DNE FOO

CONFIG /dne/first.conf
CONFIG second.conf

REJECT
""",
        KEY_SETUP: setup_validation_errors,
        KEY_EXIT: 1,
        KEY_ERRORS: [
            {
                "Reason": "Final node is already defined in the DAG",
                "SourceFile": "VALIDATION_ERRORS.dag",
                "SourceLine": 3,
                "DagCommand": "FINAL",
            },
            {
                "Reason": "Provisioner node is already defined in the DAG",
                "SourceFile": "VALIDATION_ERRORS.dag",
                "SourceLine": 5,
                "DagCommand": "PROVISIONER",
            },
            {
                "Reason": "ALL_NODES can not be used with DONE command",
                "SourceFile": "VALIDATION_ERRORS.dag",
                "SourceLine": 9,
                "DagCommand": "DONE",
            },
            {
                "Reason": "Failed to read splice NO_FILE: Provided file path does not exist",
                "SourceFile": "VALIDATION_ERRORS.dag",
                "SourceLine": 11,
                "DagCommand": "SPLICE",
            },
            {
                "Reason": "Spliced DAGs can not contain a final node",
                "SourceFile": "splice-with-final.dag",
                "SourceLine": 1,
                "DagCommand": "FINAL",
            },
            {
                "Reason": "Missing node name",
                "SourceFile": "sub-sub-splice.dag",
                "SourceLine": 1,
                "DagCommand": "JOB"
            },
            {
                "Reason": "Recursive splicing detected",
                "SourceFile": "recursive.dag",
                "SourceLine": 1,
                "DagCommand": "SPLICE",
            },
            {
                "Reason": "Failed to read include file: Provided file path does not exist",
                "SourceFile": "VALIDATION_ERRORS.dag",
                "SourceLine": 17,
                "DagCommand": "INCLUDE",
            },
            {
                "Reason": "Recursive file inclusion detected",
                "SourceFile": "recursive.inc",
                "SourceLine": 1,
                "DagCommand": "INCLUDE",
            },
            {
                "Reason": "Node A is already defined in the DAG",
                "SourceFile": "VALIDATION_ERRORS.dag",
                "SourceLine": 21,
                "DagCommand": "JOB",
            },
            {
                "Reason": "Node S1 is already defined in the DAG",
                "SourceFile": "VALIDATION_ERRORS.dag",
                "SourceLine": 22,
                "DagCommand": "JOB",
            },
            {
                "Reason": "Splice name A is already defined in the DAG",
                "SourceFile": "VALIDATION_ERRORS.dag",
                "SourceLine": 23,
                "DagCommand": "SPLICE",
            },
            {
                "Reason": "References to undefined nodes: Parents [DNE]",
                "SourceFile": "VALIDATION_ERRORS.dag",
                "SourceLine": 25,
                "DagCommand": "PARENT",
            },
            {
                "Reason": "References to undefined nodes: Children [DNE]",
                "SourceFile": "VALIDATION_ERRORS.dag",
                "SourceLine": 26,
                "DagCommand": "PARENT",
            },
            {
                "Reason": "References to undefined nodes: Parents [DNE] Children [DNE]",
                "SourceFile": "VALIDATION_ERRORS.dag",
                "SourceLine": 27,
                "DagCommand": "PARENT",
            },
            {
                "Reason": "FINAL node F can not have dependencies",
                "SourceFile": "VALIDATION_ERRORS.dag",
                "SourceLine": 28,
                "DagCommand": "PARENT",
            },
            {
                "Reason": "PROVISIONER node P can not have dependencies",
                "SourceFile": "VALIDATION_ERRORS.dag",
                "SourceLine": 28,
                "DagCommand": "PARENT",
            },
            {
                "Reason": "SERVICE node S can not have dependencies",
                "SourceFile": "VALIDATION_ERRORS.dag",
                "SourceLine": 28,
                "DagCommand": "PARENT",
            },
            {
                "Reason": "Cannot be applied to splice S1",
                "SourceFile": "VALIDATION_ERRORS.dag",
                "SourceLine": 30,
                "DagCommand": "RETRY",
            },
            {
                "Reason": "References undefined node DNE",
                "SourceFile": "VALIDATION_ERRORS.dag",
                "SourceLine": 31,
                "DagCommand": "PRIORITY",
            },
            {
                "Reason": "References to undefined nodes: DNE",
                "SourceFile": "VALIDATION_ERRORS.dag",
                "SourceLine": 33,
                "DagCommand": "CATEGORY",
            },
            {
                "Reason": "Configuration file /dne/first.conf does not exist",
                "SourceFile": "VALIDATION_ERRORS.dag",
                "SourceLine": 35,
                "DagCommand": "CONFIG",
            },
            {
                "Reason": "DAG configuration file is already defined",
                "SourceFile": "VALIDATION_ERRORS.dag",
                "SourceLine": 36,
                "DagCommand": "CONFIG",
            },
            {
                "Reason": "DAG marked with REJECT command",
                "SourceFile": "VALIDATION_ERRORS.dag",
                "SourceLine": 38,
            },
        ],
    },
    # Test Case: Provided DAG file DNE
    "NO_DAG": {
        KEY_EXIT: 1,
        KEY_ERRORS: [
            {
                "Reason": "Failed to open DAG NO_DAG.dag: Provided file path does not exist",
                "SourceFile": "NO_DAG.dag",
                "SourceLine": 0,
            }
        ]
    },
    # Test Case: Statistics for shiskabob DAG
    "SHISKABOB": {
        KEY_DAG: """#Shiskabob DAG
JOB A desc
JOB B desc
JOB C desc

PARENT A CHILD B
PARENT B CHILD C
""",
        KEY_EXIT: 0,
        KEY_STATS: {
            "TotalNodes": 3,
            "NumNodes": 3,
            "NumArcs": 2,
            "GraphHeight": 3,
            "GraphWidth": 1,
            "WidthByDepth": [1,1,1],
        }
    },
    # Test Case: Statistics for diamond DAG
    "DIAMOND": {
        KEY_DAG: """# Classic diamond DAG shape
JOB A desc
JOB B desc
JOB C desc
JOB D desc

PARENT A CHILD B C
PARENT B C CHILD D
""",
        KEY_EXIT: 0,
        KEY_STATS: {
            "TotalNodes": 4,
            "NumNodes": 4,
            "NumArcs": 4,
            "GraphHeight": 3,
            "GraphWidth": 2,
            "WidthByDepth": [1,2,1],
        }
    },
    # Test Case: Statistics for 'complex' DAG
    "COMPLEX_STATS": {
        KEY_DAG: """# Complex DAG Structure
FINAL fin desc
PROVISIONER BANK desc

SERVICE SER-1 desc
SERVICE SER-2 desc
SERVICE SER-3 desc

JOB A desc
JOB B desc
JOB C desc
JOB D desc

SUBDAG EXTERNAL SUB-1 sub.dag
SUBDAG EXTERNAL SUB-2 sub.dag
SUBDAG EXTERNAL SUB-3 sub.dag

SPLICE S1 splice.dag
SPLICE S2 splice.dag

PARENT A CHILD B
PARENT B CHILD C D
PARENT C D CHILD SUB-1 SUB-2 SUB-3
PARENT SUB-1 SUB-2 SUB-3 CHILD S1 S2
""",
        KEY_SETUP: setup_complex_dag,
        KEY_EXIT: 0,
        KEY_STATS: {
            "TotalNodes": 20,
            "NumNodes": 10,
            "NumJoinNodes": 2,
            "NumServiceNodes": 3,
            "NumSubDAGs": 3,
            "NumSplices": 4,
            "FinalNode": "fin",
            "ProvisionerNode": "BANK",
            "NumArcs": 17,
            "GraphHeight": 8,
            "GraphWidth": 4,
            "WidthByDepth": [1,1,2,1,3,1,4,2],
        }
    },
    # Test Case: Check for cycle in DAG (No Initial nodes)
    "CYCLE_NO_INITIAL": {
        KEY_DAG: """# DAG cycle with no initial nodes
JOB A desc
JOB B desc

PARENT A CHILD B
PARENT B CHILD A
""",
        KEY_EXIT: 1,
        KEY_STATS: {
            "TotalNodes": 2,
            "NumNodes": 2,
            "NumArcs": 2,
            "IsCyclic": True,
            "DetectedCycle": "No initial nodes discovered",
        },
    },
    # Test Case: Check for cycle in DAG (Disjointed section has no begining)
    "CYCLE_DISJOINTED": {
        KEY_DAG: """# DAG with disjointed cyclic portion
JOB A desc
JOB B desc
JOB C desc

PARENT A CHILD B
PARENT B CHILD A
""",
        KEY_EXIT: 1,
        KEY_STATS: {
            "TotalNodes": 3,
            "NumNodes": 3,
            "NumArcs": 2,
            "IsCyclic": True,
            "DetectedCycle": "Failed to visit nodes A,B",
        }
    },
    # Test Case: Check for cycle in DAG (Known dependency chain that loops)
    "CYCLE_CHAIN": {
        KEY_DAG: """# Cyclic DAG with well defined chain
JOB A desc
JOB B desc
JOB C desc

PARENT A CHILD B
PARENT B CHILD C
PARENT C CHILD B
""",
        KEY_EXIT: 1,
        KEY_STATS: {
            "TotalNodes": 3,
            "NumNodes": 3,
            "NumArcs": 3,
            "IsCyclic": True,
            "DetectedCycle": "A -> B -> C -> B",
        }
    },
    # Test Case: Verify join nodes
    "OPT_USE_JOIN_STATS": {
        KEY_DAG: """# Simple DAG that makes a join node
JOB A desc
JOB B desc
JOB C desc
JOB D desc
JOB E desc
JOB F desc

PARENT A B C CHILD D E F
""",
        KEY_OPTIONS: ["-joinNodes"],
        KEY_EXIT: 0,
        KEY_STATS: {
            "TotalNodes": 7,
            "NumNodes": 6,
            "NumJoinNodes": 1,
            "NumArcs": 6,
            "GraphHeight": 3,
            "GraphWidth": 3,
            "WidthByDepth": [3,1,3],
        }
    },
    # Test Case: Verify stats for previous DAG with no join nodes
    "OPT_NO_JOIN_STATS": {
        KEY_DAG: """# Simple DAG that makes a join node
JOB A desc
JOB B desc
JOB C desc
JOB D desc
JOB E desc
JOB F desc

PARENT A B C CHILD D E F
""",
        KEY_OPTIONS: ["-NoJoinNodes"],
        KEY_EXIT: 0,
        KEY_STATS: {
            "TotalNodes": 6,
            "NumNodes": 6,
            "NumArcs": 9,
            "GraphHeight": 2,
            "GraphWidth": 3,
            "WidthByDepth": [3,3],
        }
    },
    # Test Case: Verify command option for allow illegal characters
    "OPT_ALLOW_ILLEGAL_CHARS": {
        KEY_DAG: """# Node with illegal character(s)
JOB A+B desc
""",
        KEY_OPTIONS: ["-AllowIllegalChars"],
        KEY_EXIT: 0,
    },
    # Test Case: Verify failure when specifying multiple printers
    "OPT_ERROR_MULTIPLE_PRINTERS": {
        KEY_OPTIONS: ["-stat"],
        KEY_EXIT: 2,
    },
    # Test Case: Verify command option for using DAG directory
    "OPT_USE_DAG_DIR": {
        KEY_EXIT: 0,
        KEY_SETUP: setup_use_dag_dir,
        KEY_OPTIONS: ["-UseDagDir"],
        KEY_OVERRIDE: "subdir/test.dag",
        KEY_STATS: {
            "TotalNodes": 1,
            "NumNodes": 1,
            "GraphHeight": 1,
            "GraphWidth": 1,
            "WidthByDepth": [1],
        }
    },
}

#------------------------------------------------------------------
@action(params={name: name for name in TEST_CASES})
def run_command(default_condor, test_dir, request):
    CASE = request.param

    # make test case directory
    case_dir = test_dir / CASE
    if case_dir.exists():
        rmtree(case_dir)
    os.mkdir(str(case_dir))

    # Get test case details
    DETAILS = TEST_CASES[CASE]

    # Change into test case directory
    with ChangeDir(CASE):
        # Root test case dag file <CASE>.dag
        DAG = DETAILS.get(KEY_OVERRIDE, f"{CASE}.dag")

        # Write DAG file contents (unless specified as Nonde)
        if DETAILS.get(KEY_DAG) is not None:
            with open(DAG, "w") as f:
                f.write(DETAILS[KEY_DAG])

        # Run specific test case setup function
        setup = DETAILS.get(KEY_SETUP)
        if setup is not None:
            setup()

        # Setup command to execute
        CMD = ["condor_dag_checker", "-json", DAG]
        CMD.extend(DETAILS.get(KEY_OPTIONS, []))

        # Run command
        p = default_condor.run_command(CMD)

        # Dump command stdout for debugging
        with open("cmd.out", "w") as f:
            f.write(p.stdout)

        # Dump command stderr for debugging
        with open("cmd.err", "w") as f:
            f.write(p.stderr)

    # Pass all test case details needed for checks
    return (DAG, p, DETAILS[KEY_EXIT], DETAILS.get(KEY_ERRORS), DETAILS.get(KEY_STATS))

#------------------------------------------------------------------
@action
def test_dag(run_command):
    return run_command[0]

@action
def process(run_command):
    return run_command[1]

@action
def expected_exit_code(run_command):
    return run_command[2]

@action
def expected_errors(run_command):
    return run_command[3]

@action
def expected_stats(run_command):
    return run_command[4]

#==================================================================
class TestDagCheckerTool:
    def test_check_exit_code(self, process, expected_exit_code):
        """Verify command produced proper exit code"""
        assert process.returncode == expected_exit_code

    def test_check_errors(self, process, expected_errors):
        """Check for all expected error messages"""
        has_json = True
        try:
            RESULT = json.loads(process.stdout)[0]
        except json.decoder.JSONDecodeError:
            has_json = False

        if expected_errors is not None:
            assert len(expected_errors) == RESULT.get("NumErrors")

            for error in RESULT["Errors"]:
                idx = expected_errors.index(error)
                del expected_errors[idx]

            assert len(expected_errors) == 0
        elif has_json:
            assert RESULT.get("NumErrors") == None
            assert RESULT.get("Errors") == None

    def test_check_stats(self, test_dag, process, expected_stats):
        """Check for specific statistics"""
        has_json = True
        try:
            RESULT = json.loads(process.stdout)[0]
        except json.decoder.JSONDecodeError:
            has_json = False

        if expected_stats is not None:
            for STAT, VAL in expected_stats.items():
                assert STAT in RESULT
                assert expected_stats[STAT] == RESULT[STAT]
        elif has_json:
            assert KEY_DAG in RESULT
            assert RESULT[KEY_DAG] == test_dag

