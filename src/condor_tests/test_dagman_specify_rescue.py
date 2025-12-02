#!/usr/bin/env pytest

# Test user specifying a rescue file by filename

from ornithology import *
import htcondor2
import os
from pathlib import Path
from typing import Callable, Optional, Any
from shutil import rmtree
import json
import glob

#------------------------------------------------------------------
TEST_FILE_EXT = "testfile"
TEST_SUBDAG_FILENAME = "sub.dag"
TEST_DAG_FILENAME = "test.dag"
CUSTOM_NAMED_RESCUE = "custom.rescue"

#------------------------------------------------------------------
# A script to touch a file using call {passed arg}.{extension}
@action
def the_job_script(test_dir):
    path = test_dir / "named_file.py"
    with open(path, "w") as f:
        f.write(f"""#!/usr/bin/env python3

import sys
from pathlib import Path

if len(sys.argv) != 2:
    print("ERROR: Script only expects 1 argument (unique filename modifier)", file=sys.stderr)
    args = " ".join(sys.argv[1:])
    print(f"       Args: '{{args}}'", file=sys.stderr)
    sys.exit(1)

p = Path(f"{{sys.argv[1]}}.{TEST_FILE_EXT}")
p.touch()
""")
    os.chmod(path, 0o755)
    return path

#------------------------------------------------------------------
# Create simple DAG with faux rescue files
def setup_simple_dag(script: Path) -> Path:
    dag_file = Path(TEST_DAG_FILENAME)
    with open(str(dag_file), "w") as f:
        f.write(f"""
SUBMIT_DESCRIPTION touch @=desc
    executable = {script}
    arguments  = "$(dag_node_name)"
    error      = job.debug
    output     = job.debug
    universe   = local
@desc

JOB A touch
JOB B touch
JOB C touch
""")

    for i in range(1, 5):
        rescue = f"{dag_file}.rescue{i:03d}"
        with open(rescue, "w") as f:
            f.write("DONE A\nDONE B\n")

    with open(CUSTOM_NAMED_RESCUE, "w") as f:
        f.write("DONE A\n")

    return dag_file

#------------------------------------------------------------------
# Create simple DAG and SUBDAG with faux rescue files
def setup_subdags(script: Path) -> Path:
    root_dag = Path(TEST_DAG_FILENAME)
    with open(str(root_dag), "w") as f:
        f.write(f"""
SUBMIT_DESCRIPTION touch @=desc
    executable = {script}
    arguments  = "$(dag_node_name)"
    error      = job.debug
    output     = job.debug
    universe   = local
@desc

JOB A touch
JOB B touch
JOB C touch

SUBDAG EXTERNAL D {TEST_SUBDAG_FILENAME}
""")

    with open(TEST_SUBDAG_FILENAME, "w") as f:
        f.write(f"""
SUBMIT_DESCRIPTION touch @=desc
    executable = {script}
    arguments  = "$(dag_node_name)"
    error      = job.debug
    output     = job.debug
    universe   = local
@desc

JOB AA touch
JOB BB touch
JOB CC touch
""")

    for i in range(1, 5):
        rescue = f"{TEST_DAG_FILENAME}.rescue{i:03d}"
        with open(rescue, "w") as f:
            f.write("DONE A\nDONE B\n")

    for i in range(1, 5):
        rescue = f"{TEST_SUBDAG_FILENAME}.rescue{i:03d}"
        with open(rescue, "w") as f:
            f.write("DONE BB\nDONE CC\n")

    with open(CUSTOM_NAMED_RESCUE, "w") as f:
        f.write("DONE A\n")

    return root_dag

#------------------------------------------------------------------
# Test case details
#     rescue: Rescue file to specify
#     rescue_num: Expected rescue number to found in root DAG metrics file
#     setup: Function to setup test case (i.e. write DAG & rescue files)
#     rescue_files: List of expected rescue files to find in test case directory
#     touch_files: List of files we expect from re-running DAGs (touched via the_job_script action script)
class Details():
    def __init__(self, rescue: str, rescue_num: int, setup: Callable[[Path], Path], rescue_files: list[str], touch_files: list[str]):
        self.rescue = rescue
        self.rescue_num = rescue_num
        self.setup = setup
        self.check_rescue_files = rescue_files
        self.check_touch_files = touch_files

#------------------------------------------------------------------
# Generator Class to produce expected rescue file names
#      base: Base DAG filename
#      ceiling: The highest rescue file number (non-inclusive)
#      old: Rescue file number to start marking files as old (inclusive)
class RescueGenerator:
    def __init__(self, base: str, ceiling: int, old: Optional[int] = None):
        self.base = base
        self.ceiling = ceiling
        self.old = old
        self.curr = 1

    def __iter__(self):
        return self

    def __next__(self):
        if self.curr < self.ceiling:
            num = self.curr
            self.curr += 1
            rescue = f"{self.base}.rescue{num:03d}"
            if self.old is not None and num >= self.old:
                rescue += ".old"
            return rescue
        raise StopIteration

#------------------------------------------------------------------
# Generator function to make all expected rescue files based on generator
# objects plus the custom named rescue file
def gen_expected_rescues(generators: list[RescueGenerator]):
    yield CUSTOM_NAMED_RESCUE
    for gen in generators:
        for rescue in gen:
            yield rescue

#------------------------------------------------------------------
TEST_CASES = {
    "RESCUE_MOST_RECENT": Details(
        f"{TEST_DAG_FILENAME}.rescue004",
        4,
        setup_simple_dag,
        list(gen_expected_rescues([RescueGenerator(TEST_DAG_FILENAME, 5)])),
        [f"C.{TEST_FILE_EXT}"]
    ),
    "RESCUE_SPECIFIC": Details(
        f"{TEST_DAG_FILENAME}.rescue002",
        2,
        setup_simple_dag,
        list(gen_expected_rescues([RescueGenerator(TEST_DAG_FILENAME, 5, 3)])),
        [f"C.{TEST_FILE_EXT}"]
    ),
    "RESCUE_CUSTOM": Details(
        CUSTOM_NAMED_RESCUE,
        0,
        setup_simple_dag,
        list(gen_expected_rescues([RescueGenerator(TEST_DAG_FILENAME, 5, 1)])),
        [f"B.{TEST_FILE_EXT}", f"C.{TEST_FILE_EXT}"]
    ),
    "RESCUE_SUBDAG_SPECIFIC": Details(
        f"{TEST_DAG_FILENAME}.rescue001",
        1,
        setup_subdags,
        list(gen_expected_rescues([RescueGenerator(TEST_DAG_FILENAME, 5, 2), RescueGenerator(TEST_SUBDAG_FILENAME, 5, 2)])),
        [f"C.{TEST_FILE_EXT}", f"AA.{TEST_FILE_EXT}"],
    ),
    "RESCUE_SUBDAG_CUSTOM": Details(
        CUSTOM_NAMED_RESCUE,
        0,
        setup_subdags,
        list(gen_expected_rescues([RescueGenerator(TEST_DAG_FILENAME, 5, 1), RescueGenerator(TEST_SUBDAG_FILENAME, 5)])),
        [f"B.{TEST_FILE_EXT}", f"C.{TEST_FILE_EXT}", f"AA.{TEST_FILE_EXT}"],
    ),
}

#==================================================================
# Submit all the DAGS in one loop
@action
def run_dags(default_condor, test_dir, the_job_script):
    RESULTS = dict()

    for CASE, DETAILS in TEST_CASES.items():
        exe_dir = Path(CASE)
        if exe_dir.exists():
            rmtree(exe_dir)
        exe_dir.mkdir()

        with ChangeDir(exe_dir):
            dag_file = DETAILS.setup(the_job_script)

            with default_condor.use_config():
                DAG = htcondor2.Submit().from_dag(str(dag_file), {"RescueFile" : DETAILS.rescue})

            HANDLE = default_condor.submit(DAG)
            RESULTS[CASE] = (HANDLE, DETAILS.check_rescue_files, DETAILS.check_touch_files, DETAILS.rescue_num)

    yield RESULTS

#------------------------------------------------------------------
@action(params={name: name for name in TEST_CASES.keys()})
def test_info(request, run_dags):
    return (request.param, run_dags[request.param][0], run_dags[request.param][1], run_dags[request.param][2], run_dags[request.param][3])

@action
def the_test_case(test_info):
    return test_info[0]

@action
def the_test_handle(test_info):
    return test_info[1]

@action
def the_expected_rescue_files(test_info):
    return test_info[2]

@action
def the_expected_touch_files(test_info):
    return test_info[3]

@action
def the_expected_rescue_num(test_info):
    return test_info[4]

#==================================================================
class FailTestException(Exception):
    """Custom exception to raise for test case check failure"""
    pass

class PassTestException(Exception):
    """Custom exception to raise for test case check passing condition"""
    pass

#------------------------------------------------------------------
class TestDAGManSpecifiedRescue:
    def test_dag_execute_in_rescue_mode(self, the_test_case, the_test_handle, the_expected_rescue_num):
        with ChangeDir(the_test_case):
            # Wait for root DAG handle to finish
            assert the_test_handle.wait(condition=ClusterState.all_complete, timeout=120)

            # Check for root DAG metrics (completed successfully and rescue num)
            with open(f"{TEST_DAG_FILENAME}.metrics", "r") as f:
                stats = json.load(f)
                assert stats["exitcode"] == 0
                assert stats["rescue_dag_number"] == the_expected_rescue_num

            # Check root DAG debug file for specific rescue file debug message
            with open(f"{TEST_DAG_FILENAME}.dagman.out", "r") as f:
                try:
                    for line in f:
                        if "Rescue DAG file specified" in line:
                            raise PassTestException
                    raise FailTestException
                except Exception as e:
                    assert isinstance(e, PassTestException)

    def test_check_rescue_files(self, the_test_case, the_expected_rescue_files):
        located_rescues = list()

        # Check all files with *.rescue* in test case directory are expected
        with ChangeDir(the_test_case):
            for rescue in glob.glob("*.rescue*"):
                assert os.path.isfile(rescue)
                assert rescue in the_expected_rescue_files
                located_rescues.append(rescue)

        # Check we found all the expected rescue files
        located_rescues.sort()
        the_expected_rescue_files.sort()
        assert located_rescues == the_expected_rescue_files

    def test_check_touch_files(self, the_test_case, the_expected_touch_files):
        located_touch_files = list()

        # Check that all *.{extension} files in test case directory are expected
        with ChangeDir(the_test_case):
            for rescue in glob.glob(f"*.{TEST_FILE_EXT}"):
                assert os.path.isfile(rescue)
                assert rescue in the_expected_touch_files
                located_touch_files.append(rescue)

        # Check we found all expected touched files
        located_touch_files.sort()
        the_expected_touch_files.sort()
        assert located_touch_files == the_expected_touch_files

