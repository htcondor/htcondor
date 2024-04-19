#!/usr/bin/env pytest

# test_dagman_script_debug_file.py
#
# This test checks DAGMans functionality to capture a scripts
# standard output streams (stdout and/or stderr) to a specified
# debug file.

from ornithology import *
import htcondor2 as htcondor
import os
from pathlib import Path

DAG_FILENAME = "test_script_debugging.dag"
SCRIPT_NAME = "output.py"
NODE_SUBDIR = "subdir"

#-----------------------------------------------------------------------------------------------
# Fixture to write a common use script that writes to stderr & stdout
@action
def write_node_script():
    with open(SCRIPT_NAME, "w") as f:
        f.write("""#!/usr/bin/python3
#Test script that writes to stdout & stderr
import sys

num_args = len(sys.argv)
if num_args != 2:
    print(f"ERROR: Invalid number of arguments ({num_args}) passed", file=sys.stderr)
    sys.exit(1)
node = sys.argv[1]
print(f"[STDOUT] Node {node}")
# Using print(msg, file=sys.stderr) did not produce the write results
sys.stderr.write(f"[STDERR] Node {node}\\n")
""")
    os.chmod(SCRIPT_NAME, 0o755)

#-----------------------------------------------------------------------------------------------
# DAG Node test information class
class NodeInfo:
    def __init__(self, name, filename, output_type, script_type, use_subdir=False):
        self.name = name
        self.debug_file = filename
        self.output_type = output_type
        self.script_type = script_type
        self.use_subdir = use_subdir

#-----------------------------------------------------------------------------------------------
@action
def run_test_dag(default_condor, test_dir, path_to_sleep, write_node_script):
    # Subdir for testing debug file is written relative to node execution
    subdir = os.path.join(str(test_dir), NODE_SUBDIR)
    if not os.path.exists(subdir):
        os.mkdir(subdir)
    else:
        for f in os.listdir(subdir):
            os.remove(f)

    full_path_debug = os.path.join(str(test_dir), "path.debug")

    # Make node test cases
    NODES = [
        NodeInfo("A", "A.debug", "STDOUT", "PRE"),
        NodeInfo("B", "B.debug", "STDERR", "POST"),
        NodeInfo("C", "multi.debug", "ALL", "PRE"),
        NodeInfo("D", "multi.debug", "ALL", "POST"),
        NodeInfo("E", "path.debug", "STDERR", "PRE", True),
        NodeInfo("F", full_path_debug, "STDOUT", "POST", True)
    ]

    # Debug writes in append so remove any existing debug files
    for node in NODES:
        if os.path.exists(node.debug_file):
            os.remove(node.debug_file)

    # Write DAG file
    with open(DAG_FILENAME, "w") as f:
        f.write(f"""
SUBMIT-DESCRIPTION sleep {{
    executable = {path_to_sleep}
    arguments  = 0
    log        = $(JOB).log
}}
""")
        for node in NODES:
            sdir = f" DIR {NODE_SUBDIR}" if node.use_subdir else ""
            script = os.path.join("..", SCRIPT_NAME) if node.use_subdir else SCRIPT_NAME
            f.write(f"\nJOB {node.name} sleep{sdir}\n")
            f.write(f"SCRIPT DEBUG {node.debug_file} {node.output_type} {node.script_type} {node.name} {script} $JOB\n")
    assert os.path.exists(DAG_FILENAME)

    # Submit DAG, wait for completion, and return node test case info
    dag = htcondor.Submit.from_dag(DAG_FILENAME)
    dagman_job = default_condor.submit(dag)
    assert dagman_job.wait(condition=ClusterState.all_complete, timeout=60)
    return NODES

#===============================================================================================
class TestDAGManScriptDebug:
    def test_dag_scripts_debug_file(self, run_test_dag):
        # Dictionary of debug files to expected lines
        check_files = {}
        for node in run_test_dag:
            # Verify that the debug file exists
            path = ""
            if os.path.isabs(node.debug_file):
                path = node.debug_file
            elif node.use_subdir:
                path = os.path.join(NODE_SUBDIR, node.debug_file)
            else:
                path = node.debug_file
            assert os.path.exists(path)

            # Create list of expected lines
            expected_lines = [f"BANNER-{node.name}"]
            if node.output_type == "STDOUT":
                expected_lines.append(f"[STDOUT] Node {node.name}")
            elif node.output_type == "STDERR":
                expected_lines.append(f"[STDERR] Node {node.name}")
            else:
                expected_lines.append(f"[STDOUT] Node {node.name}")
                expected_lines.append(f"[STDERR] Node {node.name}")

            if path in check_files.keys():
                check_files[path].extend(expected_lines)
            else:
                check_files.update({path : expected_lines})

        unexpected_lines = []
        # Read files and check for expected lines
        for path, expected_lines in check_files.items():
            with open(path, "r") as f:
                for line in f:
                    line = line.strip()
                    if len(line) == 0:
                        continue
                    # User banner line to get node name
                    if line.startswith("***"):
                        s_pos = line.find("Node=")
                        e_pos = line.find(" ", s_pos)
                        node = line[s_pos+5:e_pos]
                        item = f"BANNER-{node}"
                        if item in expected_lines:
                            expected_lines.remove(item)
                    # Expected line
                    elif line in expected_lines:
                        expected_lines.remove(line)
                    # Unexpected line
                    else:
                        unexpected_lines.append(line)
                # Make sure all expected lines were located
                assert len(expected_lines) == 0
        # Make sure no unexpected lines appeared
        assert len(unexpected_lines) == 0

