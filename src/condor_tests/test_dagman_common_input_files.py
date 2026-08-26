#!/usr/bin/env pytest

# Verify that a DAG whose non-container nodes declare differing
# MY.CommonInputFiles values gets rejected -- both by DAGMan itself at
# submit time (DirectSubmit path -- ShellSubmit does not perform this
# check yet) and by the condor_dag_checker linter -- while a DAG where
# every node agrees on the same list is accepted by both.
#
# Also covers two condor_dag_checker -CheckJDL corners:
#  - The same mismatch/match detection, but with the submit descriptions
#    written inline ("JOB name { ... }") instead of as external .sub files.
#  - A SUBDAG node is its own DAGMan/schedd submission, so its
#    CommonInputFiles must NOT be compared against its parent's.
#
# Speed notes:
#  - The nodes have no PARENT/CHILD relationship: the CommonInputFiles
#    check fires purely at submit time, so nothing needs to run before
#    the second node's (rejected, in the mismatch case) submission is
#    attempted.
#  - Both test classes share one class-scoped Condor pool instead of
#    standing one up per class (@standup/@action fixtures are scope="class").
#  - Jobs stay on the default (vanilla) universe: CommonInputFiles is a
#    file-transfer feature, and local universe skips file transfer
#    entirely, so it isn't a valid substitute here.

from ornithology import *
import json

import htcondor2

CIF_MISMATCH_ERROR = "Multiple differing common input transfer lists declared in DAG"

TEST_CASES = {
    "match": {
        "cif_a": "shared_data.tar.gz",
        "cif_b": "shared_data.tar.gz",
    },
    "mismatch": {
        "cif_a": "shared_data.tar.gz",
        "cif_b": "other_data.tar.gz",
    },
}


@standup
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor",
        config={"DAGMAN_MAX_SUBMIT_ATTEMPTS": "1"},
    ) as condor:
        yield condor


def write_node_submit(name, executable, cif):
    filename = f"{name}.sub"
    with open(filename, "w") as f:
        f.write(f"""
        executable = {executable}
        arguments  = 0
        log        = {name}.log
        MY.CommonInputFiles = "{cif}"
        queue
        """)
    return filename


def write_dag(sub_a, sub_b):
    filename = "test.dag"
    with open(filename, "w") as f:
        # No PARENT/CHILD: both nodes are ready immediately, so DAGMan
        # attempts both submissions right away instead of serializing them.
        f.write(f"""
        JOB A {sub_a}
        JOB B {sub_b}
        """)
    return filename


@action(params={name: name for name in TEST_CASES})
def dag_case(test_dir, path_to_sleep, request):
    case = request.param
    case_dir = test_dir / case
    case_dir.mkdir(exist_ok=True)

    with ChangeDir(case_dir):
        sub_a = write_node_submit("A", path_to_sleep, TEST_CASES[case]["cif_a"])
        sub_b = write_node_submit("B", path_to_sleep, TEST_CASES[case]["cif_b"])
        dag_file = write_dag(sub_a, sub_b)

    return case, case_dir, dag_file


@action
def dagman_out_lines(condor, dag_case):
    case, case_dir, dag_file = dag_case

    with ChangeDir(case_dir):
        # SubmitMethod 1 == DirectSubmit; ShellSubmit doesn't do this check yet.
        dag = htcondor2.Submit.from_dag(dag_file, options={"SubmitMethod": 1})
        handle = condor.submit(dag)
        handle.wait(condition=ClusterState.all_terminal, timeout=90)
        condor.job_queue.wait_for_job_completion(handle.job_ids)

    with open(str(case_dir / f"{dag_file}.dagman.out"), "r") as f:
        lines = f.readlines()
    return case, lines


@action
def dag_checker_result(condor, dag_case):
    case, case_dir, dag_file = dag_case

    with ChangeDir(case_dir):
        proc = condor.run_command(["condor_dag_checker", dag_file, "-json", "-CheckJDL", "-strict"])
    return case, proc


class TestDAGManCommonInputFiles:
    def test_dagman_common_input_files(self, dagman_out_lines):
        case, lines = dagman_out_lines
        error_seen = any(CIF_MISMATCH_ERROR in line for line in lines)
        if case == "mismatch":
            assert error_seen, "DAGMan should have rejected the mismatched MY.CommonInputFiles submission"
        else:
            assert not error_seen, "DAGMan should not flag matching MY.CommonInputFiles values"

    def test_dag_checker_common_input_files(self, dag_checker_result):
        case, proc = dag_checker_result
        result = json.loads(proc.stdout)[0]
        if case == "mismatch":
            assert proc.returncode == 1
            assert any(
                CIF_MISMATCH_ERROR in error.get("Reason", "")
                for error in result.get("Errors", [])
            ), f"condor_dag_checker did not flag the CommonInputFiles mismatch: {proc.stdout}"
        else:
            assert proc.returncode == 0
            assert result.get("NumErrors", 0) == 0, f"condor_dag_checker unexpectedly flagged errors: {proc.stdout}"


#------------------------------------------------------------------
# Same match/mismatch coverage as above, but the submit descriptions are
# written inline ("JOB name { ... }") in the DAG file instead of as
# external .sub files referenced by path -- only condor_dag_checker is
# exercised here since the DirectSubmit behavior is already covered above
# and doesn't differ based on where the submit description text lives.

def write_inline_dag(executable, cif_a, cif_b):
    filename = "inline.dag"
    with open(filename, "w") as f:
        f.write(f"""
        JOB A {{
            executable = {executable}
            arguments  = 0
            log        = A.log
            MY.CommonInputFiles = "{cif_a}"
            queue
        }}
        JOB B {{
            executable = {executable}
            arguments  = 0
            log        = B.log
            MY.CommonInputFiles = "{cif_b}"
            queue
        }}
        """)
    return filename


@action(params={name: name for name in TEST_CASES})
def inline_dag_case(test_dir, path_to_sleep, request):
    case = request.param
    case_dir = test_dir / f"inline_{case}"
    case_dir.mkdir(exist_ok=True)

    with ChangeDir(case_dir):
        dag_file = write_inline_dag(path_to_sleep, TEST_CASES[case]["cif_a"], TEST_CASES[case]["cif_b"])

    return case, case_dir, dag_file


@action
def inline_dag_checker_result(condor, inline_dag_case):
    case, case_dir, dag_file = inline_dag_case

    with ChangeDir(case_dir):
        proc = condor.run_command(["condor_dag_checker", dag_file, "-json", "-CheckJDL", "-strict"])
    return case, proc


class TestDagCheckerInlineCommonInputFiles:
    def test_dag_checker_inline_common_input_files(self, inline_dag_checker_result):
        case, proc = inline_dag_checker_result
        result = json.loads(proc.stdout)[0]
        if case == "mismatch":
            assert proc.returncode == 1
            assert any(
                CIF_MISMATCH_ERROR in error.get("Reason", "")
                for error in result.get("Errors", [])
            ), f"condor_dag_checker did not flag the inline CommonInputFiles mismatch: {proc.stdout}"
        else:
            assert proc.returncode == 0
            assert result.get("NumErrors", 0) == 0, f"condor_dag_checker unexpectedly flagged errors on matching inline JDL: {proc.stdout}"


#------------------------------------------------------------------
# A SUBDAG node runs as its own DAGMan/schedd submission, so its
# CommonInputFiles must never be compared against its parent's -- confirm
# condor_dag_checker treats the two as separate scopes and doesn't flag
# a false mismatch.

def write_subdag_scope_case(executable):
    with open("sub.dag", "w") as f:
        f.write(f"""
        JOB S {{
            executable = {executable}
            arguments  = 0
            log        = S.log
            MY.CommonInputFiles = "child_data.tar.gz"
            queue
        }}
        """)

    filename = "subdag_scope.dag"
    with open(filename, "w") as f:
        f.write(f"""
        JOB A {{
            executable = {executable}
            arguments  = 0
            log        = A.log
            MY.CommonInputFiles = "parent_data.tar.gz"
            queue
        }}
        SUBDAG EXTERNAL SUB sub.dag
        """)
    return filename


@action
def subdag_scope_case_dir(test_dir, path_to_sleep):
    case_dir = test_dir / "subdag_scope"
    case_dir.mkdir(exist_ok=True)

    with ChangeDir(case_dir):
        dag_file = write_subdag_scope_case(path_to_sleep)

    return case_dir, dag_file


@action
def subdag_scope_dag_checker_result(condor, subdag_scope_case_dir):
    case_dir, dag_file = subdag_scope_case_dir

    with ChangeDir(case_dir):
        proc = condor.run_command(["condor_dag_checker", dag_file, "-json", "-CheckJDL", "-CheckSubDags", "-strict"])
    return proc


class TestDagCheckerSubdagCommonInputFilesScope:
    def test_dag_checker_subdag_common_input_files_no_conflict(self, subdag_scope_dag_checker_result):
        proc = subdag_scope_dag_checker_result
        result = json.loads(proc.stdout)[0]
        assert proc.returncode == 0
        assert result.get("NumErrors", 0) == 0, (
            "condor_dag_checker incorrectly flagged a CommonInputFiles conflict between a "
            f"parent node and a SUBDAG's own scope: {proc.stdout}"
        )
