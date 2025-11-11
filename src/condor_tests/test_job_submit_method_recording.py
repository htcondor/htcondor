#!/usr/bin/env pytest

from ornithology import *
import htcondor2
import os
import pytest
from time import time as now
from time import sleep
from pathlib import Path
from shutil import rmtree
from typing import Callable, Optional, Any

#-----------------------------------------------------------------------------------------
class SubmissionError(Exception):
        """Custom exception to raise in job submission helper functions upon failure."""
        def __init__(self, message="Failed to submit job(s)"):
            self.message = message
            super().__init__(self.message)

#-----------------------------------------------------------------------------------------
TEST_CASE_ATTR = "TestCase"
TEST_CASE_ENV = "TEST_CASE"

FILENAME_SUB = "sleep.sub"
FILENAME_DAG = "test.dag"
FILENAME_JOBSET = "job.set"

JOB_LOG = "job.log"

KEY_VAL = "value"
KEY_OVERRIDE = "override"
KEY_NOUN = "noun"

#-----------------------------------------------------------------------------------------
@standup
def the_condor(test_dir):
    dagman_append = test_dir / f".{FILENAME_DAG}.append"
    with open(dagman_append, "w") as f:
        f.write(f"""
submit_event_user_notes = $ENV({TEST_CASE_ENV})
My.{TEST_CASE_ATTR} = "$ENV({TEST_CASE_ENV})"
""")

    with Condor(test_dir / "the_condor", config={"USE_JOBSETS": True, "DAGMAN_INSERT_SUB_FILE": dagman_append}) as condor:
        yield condor

#-----------------------------------------------------------------------------------------
def write_submit(sleep: Path) -> Path:
    path = Path(FILENAME_SUB)
    with open(path, "w") as f:
        f.write(f"""# Simple sleep job
executable = {sleep}
arguments  = 0
log = {JOB_LOG}

submit_event_user_notes = $ENV({TEST_CASE_ENV})

My.{TEST_CASE_ATTR} = "$ENV({TEST_CASE_ENV})"

queue
""")

    return path

def write_dag(sleep: Path, sleep_submit: Path) -> Path:
    path = Path(FILENAME_DAG)
    with open(path, "w") as f:
        f.write(f"""# Simple DAG file
ENV GET {TEST_CASE_ENV}

SUBMIT-DESCRIPTION sleep @=desc
    executable = {sleep}
    arguments  = 0
    submit_event_user_notes = $ENV({TEST_CASE_ENV})
    My.{TEST_CASE_ATTR} = "$ENV({TEST_CASE_ENV})"
    queue
@desc

JOB DESC sleep
JOB INLINE @=desc
    executable = {sleep}
    arguments  = 0
    submit_event_user_notes = $ENV({TEST_CASE_ENV})
    My.{TEST_CASE_ATTR} = "$ENV({TEST_CASE_ENV})"
    queue
@desc
JOB EXTERNAL {sleep_submit}
""")

    return path

def prepare_job(sleep: Path) -> Path:
    return write_submit(sleep)

def prepare_dag(sleep: Path) -> Path:
    sub = write_submit(sleep)
    return write_dag(sleep, sub)

def prepare_jobset(sleep: Path) -> Path:
    path = Path(FILENAME_JOBSET)
    with open(path, "w") as f:
        f.write(f"""
name = JobSubmitMethodTest

iterator = table var {{
     Job1
     Job2
}}

job {{
     executable = {sleep}
     arguments  = 0
     log        = {JOB_LOG}

     submit_event_user_notes = $ENV({TEST_CASE_ENV})
     My.{TEST_CASE_ATTR} = "$ENV({TEST_CASE_ENV})"

     queue
}}
""")

    return path

#-----------------------------------------------------------------------------------------
def check_undefined(condor: Condor, test: Path, **kwargs) -> None:
    try:
        with condor.use_config():
            with open(test, "r") as f:
                desc = htcondor2.Submit(f.read())
            desc.setSubmitMethod(-1, True)
            schedd = htcondor2.Schedd()
            result = schedd.submit(desc)
    except Exception as e:
        raise SubmissionError(f"Failed to submit via python API: {e}")

def check_condor_submit(condor: Condor, test: Path, **kwargs) -> None:
    p = condor.run_command(["condor_submit", test])
    if p.returncode != 0:
        raise SubmissionError(f"Failed to submit via condor_submit:\n{p.stdout}\n{p.stderr}")

def check_submit_dag_direct(condor: Condor, test: Path, **kwargs) -> None:
    p = condor.run_command(["condor_submit_dag", "-SubmitMethod", "1", test])
    if p.returncode != 0:
        raise SubmissionError(f"Failed to submit via condor_dag_submit:\n{p.stdout}\n{p.stderr}")

def check_submit_dag_shell(condor: Condor, test: Path, **kwargs) -> None:
    p = condor.run_command(["condor_submit_dag", "-SubmitMethod", "0", test])
    if p.returncode != 0:
        raise SubmissionError(f"Failed to submit via condor_dag_submit:\n{p.stdout}\n{p.stderr}")

def check_dag_py_submit(condor: Condor, test: Path, **kwargs) -> None:
    try:
        with condor.use_config():
            desc = htcondor2.Submit.from_dag(str(test))
            schedd = htcondor2.Schedd()
            result = schedd.submit(desc)
    except Exception as e:
        raise SubmissionError(f"Failed to submit via python API: {e}")

def check_py_submit(condor: Condor, test: Path, **kwargs) -> str:
    ret = "UhOh..."

    try:
        with condor.use_config():
            with open(test, "r") as f:
                desc = htcondor2.Submit(f.read())
            try:
                val = kwargs.get(KEY_VAL)
                override = kwargs.get(KEY_OVERRIDE, False)
                if val is not None:
                    desc.setSubmitMethod(val, override)
                ret = str(desc.getSubmitMethod())
            except ValueError as e:
                ret = str(e)

            schedd = htcondor2.Schedd()
            result = schedd.submit(desc)
    except Exception as e:
        raise SubmissionError(f"Failed to submit via python API: {e}")

    return ret

def check_htc_submit(condor: Condor, test: Path, **kwargs) -> None:
    noun = kwargs.get(KEY_NOUN, "NO-NOUN")
    p = condor.run_command(["htcondor", noun, "submit", test])
    if p.returncode != 0:
        raise SubmissionError(f"Failed to submit via htcondor {noun} submit:\n{p.stdout}\n{p.stderr}")

#-----------------------------------------------------------------------------------------
class Details():
    def __init__(self, prep: Callable[[Path], Path], exe: Callable[[Condor, str], Optional[str]], result: dict, extra: dict = {}, out: Optional[str] = None, is_dag: bool = False):
        self.prepare = prep
        self.execute = exe
        self.expected = result
        self.log = f"{FILENAME_DAG}.dagman.log" if is_dag else JOB_LOG
        self.check = out
        self.details = extra

TEST_CASES = {
    "UNDEFINED": Details(prepare_job, check_undefined, {None:1}),
    "CONDOR_SUBMIT": Details(prepare_job, check_condor_submit, {0:1}),
    "DAGMAN_DIRECT": Details(prepare_dag, check_submit_dag_direct, {0:1, 1:3}, is_dag=True),
    "DAGMAN_SHELL": Details(prepare_dag, check_submit_dag_shell, {0:4}, is_dag=True),
    "PY_DAG": Details(prepare_dag, check_dag_py_submit, {1:3, 2:1}, is_dag=True),
    "PY_DEFAULT": Details(prepare_job, check_py_submit, {2:1}, out="2"),
    "PY_INVALID": Details(prepare_job, check_py_submit, {2:1}, extra={KEY_VAL:69}, out="Submit method value must be 100 or greater, or allowed_reserved_values must be True."),
    "PY_OVERRIDE": Details(prepare_job, check_py_submit, {69:1}, extra={KEY_VAL:69, KEY_OVERRIDE:True}, out="69"),
    "PY_SET_100": Details(prepare_job, check_py_submit, {100:1}, extra={KEY_VAL:100}, out="100"),
    "PY_SET": Details(prepare_job, check_py_submit, {666:1}, extra={KEY_VAL:666}, out="666"),
    "HTC_JOB_SUBMIT": Details(prepare_job, check_htc_submit, {3:1}, extra={KEY_NOUN:"job"}),
    "HTC_DAG_SUBMIT": Details(prepare_dag, check_htc_submit, {4:1, 1:3}, extra={KEY_NOUN:"dag"}, is_dag=True),
    "HTC_JOBSET_SUBMIT": Details(prepare_jobset, check_htc_submit, {5:2}, extra={KEY_NOUN:"jobset"}),
}

#-----------------------------------------------------------------------------------------
@action
def run_dags(the_condor, test_dir, path_to_sleep):
    TESTS = dict()

    for TEST, SETUP in TEST_CASES.items():
        case_dir = test_dir / TEST
        if case_dir.exists():
            rmtree(case_dir)

        os.mkdir(case_dir)

        with ChangeDir(TEST):
            try:
                test_file = SETUP.prepare(path_to_sleep)
                os.environ[TEST_CASE_ENV] = TEST
                output = SETUP.execute(the_condor, test_file, **SETUP.details)
                del os.environ[TEST_CASE_ENV]

                TESTS[TEST] = (SETUP.log, SETUP.expected, SETUP.check, output)
            except SubmissionError as e:
                TESTS[TEST] = e

    yield TESTS

#-----------------------------------------------------------------------------------------
@action(params={name: name for name in TEST_CASES})
def test_info(request, run_dags):
    details = run_dags[request.param]

    if isinstance(details, SubmissionError):
        pytest.fail(str(details))

    assert isinstance(details, tuple)

    return (
        request.param,    # Test name
        details[0],       # Test log to follow
        details[1],       # Test expected JobSubmitMethod values -> Count
        details[2],       # Test expected output (None == skip)
        details[3],       # Test output
    )

@action
def test_case(test_info):
    return test_info[0]

@action
def test_log(test_info):
    return test_info[1]

@action
def test_expected(test_info):
    return test_info[2]

@action
def test_check(test_info):
    return test_info[3]

@action
def test_output(test_info):
    return test_info[4]

#-----------------------------------------------------------------------------------------
@action
def test_wait(test_case, test_log) -> None:
    with ChangeDir(test_case):
        jel = htcondor2.JobEventLog(test_log)
        start = now()
        counts = [0, 0]
        while True:
            assert now() - start <= 90, f"ERROR: Test case {test_case} job(s) timed out"
            for event in jel.events(stop_after=0):
                if event.type == htcondor2.JobEventType.SUBMIT:
                    assert event.get("LogNotes") == test_case
                    counts[0] += 1
                elif event.type == htcondor2.JobEventType.JOB_TERMINATED:
                    assert event.get("ReturnValue") == 0
                    counts[1] += 1
                    if len(set(counts)) == 1:
                        return

@action
def test_get_counts(the_condor, test_case, test_wait, test_expected) -> dict:
    schedd = the_condor.get_local_schedd()

    expected_num_ads = sum(test_expected.values())
    attempts = 0
    history = []

    while len(history) != expected_num_ads:
        if attempts > 0:
            sleep(2)

        attempts += 1
        assert attempts <= 10, f"ERROR: Failed to get history all {expected_num_ads} records for test case {test_case}"
        history = schedd.history(
            constraint=f'TestCase=="{test_case}"',
            projection=["JobSubmitMethod"],
        )

    results = dict()
    for ad in history:
        jsm = ad.get("JobSubmitMethod")
        if jsm in results:
            results[jsm] += 1
        else:
            results.update({jsm : 1})

    return results

#=========================================================================================
class TestJobSubmitMethod:
    def test_check_value_counts(self, test_get_counts, test_expected):
        assert test_get_counts == test_expected

    def test_check_output(self, test_check, test_output):
        if test_check is None:
            pytest.skip("Test case does not check output")
        assert test_check == test_output
