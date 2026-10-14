#!/usr/bin/env pytest

# Verify that a DAG whose non-container nodes declare differing
# CommonInputFiles values gets rejected -- both by DAGMan itself at
# submit time (DirectSubmit and ShellSubmit paths) and by the
# condor_dag_checker linter -- while a DAG where every node agrees on the
# same list is accepted by both. Every case is run with the list declared
# as the MY.CommonInputFiles job attribute, via the first class
# common_input_files submit command, via its transfer_common_input alias,
# and with the two nodes mixing the attribute and the submit command (which
# must produce identical lists).
#
# DAGMan is additionally checked with a shared submit file whose
# CommonInputFiles comes from node VARS, so the list is only known once
# DAGMan applies node variables at submit time. condor_dag_checker does
# not apply node VARS, so those cases are DAGMan only.
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
#  - Every DAGMan case's DAG is submitted up front by a single fixture so
#    they all run concurrently; each parametrized case only waits on its own.
#  - Both test classes share one class-scoped Condor pool instead of
#    standing one up per class (@standup/@action fixtures are scope="class").
#  - Jobs stay on the default (vanilla) universe: CommonInputFiles is a
#    file-transfer feature, and local universe skips file transfer
#    entirely, so it isn't a valid substitute here.

from ornithology import *
import itertools
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

# Same lists as above, but supplied to a shared submit file via node VARS
VARS_TEST_CASES = {
    "vars_match": {
        "cif_a": "shared_data.tar.gz",
        "cif_b": "shared_data.tar.gz",
    },
    "vars_mismatch": {
        "cif_a": "shared_data.tar.gz",
        "cif_b": "other_data.tar.gz",
    },
}

# DAGMan SubmitMethod values (DagSubmitMethod): both must enforce the check
SUBMIT_METHODS = {
    "direct": 1,
    "shell": 0,
}

# Ways a submit description can declare its common input files: setting the
# job attribute directly or using the first class submit command (or its alias)
CIF_COMMANDS = {
    "attr": 'MY.CommonInputFiles = "{cif}"',
    "jdl": "common_input_files = {cif}",
    "jdl_alt": "transfer_common_input = {cif}",
}

# Declaration form used by nodes (A, B) for each run: either the same form
# for both nodes, or mixed forms that must still produce identical lists
CIF_FORMS = {
    "attr": ("attr", "attr"),
    "jdl": ("jdl", "jdl"),
    "jdl_alt": ("jdl_alt", "jdl_alt"),
    "mixed": ("attr", "jdl"),
}


@standup
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor",
        config={"DAGMAN_MAX_SUBMIT_ATTEMPTS": "1"},
    ) as condor:
        yield condor


def write_node_submit(name, executable, cif, cif_command):
    filename = f"{name}.sub"
    cif_line = CIF_COMMANDS[cif_command].format(cif=cif)
    with open(filename, "w") as f:
        f.write(f"""
        executable = {executable}
        arguments  = 0
        log        = {name}.log
        {cif_line}
        queue
        """)
    return filename


def write_dag(sub_a, sub_b, node_vars=None):
    filename = "test.dag"
    with open(filename, "w") as f:
        # No PARENT/CHILD: both nodes are ready immediately, so DAGMan
        # attempts both submissions right away instead of serializing them.
        f.write(f"""
        JOB A {sub_a}
        JOB B {sub_b}
        """)
        for node, value in (node_vars or {}).items():
            f.write(f'VARS {node} data_file="{value}"\n')
    return filename


@action(params={name: name for name in CIF_FORMS})
def cif_form(request):
    return request.param


@action(params={name: name for name in TEST_CASES})
def dag_case(test_dir, path_to_sleep, cif_form, request):
    case = request.param
    case_dir = test_dir / f"{cif_form}_{case}"
    case_dir.mkdir(exist_ok=True)
    form_a, form_b = CIF_FORMS[cif_form]

    with ChangeDir(case_dir):
        sub_a = write_node_submit("A", path_to_sleep, TEST_CASES[case]["cif_a"], form_a)
        sub_b = write_node_submit("B", path_to_sleep, TEST_CASES[case]["cif_b"], form_b)
        dag_file = write_dag(sub_a, sub_b)

    return case, case_dir, dag_file


# Every DAGMan run: (submit method, cif form, case) keyed by a readable test id
# Note: ornithology forbids '-' in param keys
DAGMAN_RUNS = {
    f"{method}_{form}_{case}": (method, form, case)
    for method, form, case in itertools.product(SUBMIT_METHODS, CIF_FORMS, {**TEST_CASES, **VARS_TEST_CASES})
}


@action
def run_dags(condor, test_dir, path_to_sleep):
    # Submit every DAG up front so they all run concurrently, then
    # each parametrized test case only waits on its own DAG
    runs = dict()

    for run_id, (method, form, case) in DAGMAN_RUNS.items():
        case_dir = test_dir / f"dagman_{method}_{form}_{case}"
        case_dir.mkdir(exist_ok=True)
        form_a, form_b = CIF_FORMS[form]

        with ChangeDir(case_dir):
            if case in VARS_TEST_CASES:
                cifs = VARS_TEST_CASES[case]
                node_vars = {"A": cifs["cif_a"], "B": cifs["cif_b"]}
                if form_a == form_b:
                    # Both nodes share one submit file: only their VARS differ
                    sub = write_node_submit("node", path_to_sleep, "$(data_file)", form_a)
                    dag_file = write_dag(sub, sub, node_vars)
                else:
                    sub_a = write_node_submit("A", path_to_sleep, "$(data_file)", form_a)
                    sub_b = write_node_submit("B", path_to_sleep, "$(data_file)", form_b)
                    dag_file = write_dag(sub_a, sub_b, node_vars)
            else:
                cifs = TEST_CASES[case]
                sub_a = write_node_submit("A", path_to_sleep, cifs["cif_a"], form_a)
                sub_b = write_node_submit("B", path_to_sleep, cifs["cif_b"], form_b)
                dag_file = write_dag(sub_a, sub_b)

            dag = htcondor2.Submit.from_dag(dag_file, options={"SubmitMethod": SUBMIT_METHODS[method]})
            handle = condor.submit(dag)

        expect_mismatch = cifs["cif_a"] != cifs["cif_b"]
        runs[run_id] = (handle, expect_mismatch, case_dir, dag_file)

    return runs


@action(params={run_id: run_id for run_id in DAGMAN_RUNS})
def dagman_run(request, run_dags):
    return run_dags[request.param]


@action
def dagman_out_lines(dagman_run):
    handle, expect_mismatch, case_dir, dag_file = dagman_run

    # All DAGs share the pool's slots, so allow longer than a lone DAG would need
    # Note: Don't use condor.job_queue.wait_for_job_completion() here. It consumes
    #       the shared job queue log, so with every DAG submitted up front an earlier
    #       wait eats the later DAGs' completion events, and they wait until timeout
    assert handle.wait(condition=ClusterState.all_terminal, timeout=300)

    with open(str(case_dir / f"{dag_file}.dagman.out"), "r") as f:
        lines = f.readlines()
    return expect_mismatch, case_dir, lines


@action
def dag_checker_result(condor, dag_case):
    case, case_dir, dag_file = dag_case

    with ChangeDir(case_dir):
        proc = condor.run_command(["condor_dag_checker", dag_file, "-json", "-CheckJDL", "-strict"])
    return case, proc


class TestDAGManCommonInputFiles:
    def test_dagman_common_input_files(self, dagman_out_lines):
        expect_mismatch, _, lines = dagman_out_lines
        error_seen = any(CIF_MISMATCH_ERROR in line for line in lines)
        if expect_mismatch:
            assert error_seen, "DAGMan should have rejected the mismatched CommonInputFiles submission"
        else:
            assert not error_seen, "DAGMan should not flag matching CommonInputFiles values"

    def test_dagman_dry_run_files_removed(self, dagman_out_lines):
        # ShellSubmit writes condor_submit -dry-run results to a temp file which
        # must be cleaned up whether or not the node passes the check
        _, case_dir, _ = dagman_out_lines
        leftover = list(case_dir.glob("*-dry-run.ads.temp"))
        assert leftover == [], f"DAGMan left dry run result files behind: {leftover}"

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

def write_inline_dag(executable, cif_a, cif_b, cif_form):
    filename = "inline.dag"
    form_a, form_b = CIF_FORMS[cif_form]
    cif_line_a = CIF_COMMANDS[form_a].format(cif=cif_a)
    cif_line_b = CIF_COMMANDS[form_b].format(cif=cif_b)
    with open(filename, "w") as f:
        f.write(f"""
        JOB A {{
            executable = {executable}
            arguments  = 0
            log        = A.log
            {cif_line_a}
            queue
        }}
        JOB B {{
            executable = {executable}
            arguments  = 0
            log        = B.log
            {cif_line_b}
            queue
        }}
        """)
    return filename


@action(params={name: name for name in TEST_CASES})
def inline_dag_case(test_dir, path_to_sleep, cif_form, request):
    case = request.param
    case_dir = test_dir / f"inline_{cif_form}_{case}"
    case_dir.mkdir(exist_ok=True)

    with ChangeDir(case_dir):
        dag_file = write_inline_dag(path_to_sleep, TEST_CASES[case]["cif_a"], TEST_CASES[case]["cif_b"], cif_form)

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
