#!/usr/bin/env pytest

#   ====test_remote_history_filters====
#   Regression tests for the remote history job/user and DAG/batch filters
#   (users, jobs, dagids, batchids, batchnames) passed in the history query ad
#   as ClassAd lists of strings and handed to the remote condor_history helper.
#
#   Covers:
#       - Python bindings argument validation and normalization (no daemon contact)
#       - Python bindings remote version gating of the filters
#       - Python -> C glue ClassAd list string formatting (quoting)
#       - Schedd side enforcement of filter values (command line injection guard)
#       - Python bindings end to end filtering (requires remote filter support)
#       - condor_history remote filter bundling

from ornithology import *

import re
import pytest

import classad2 as classad
import htcondor2 as htcondor

from htcondor2 import HTCondorException
from htcondor2.htcondor2_impl import _history_query
import htcondor2._history_helper as history_helper
from htcondor2._history_helper import (
    CMD_SCHEDD_HISTORY,
    HistorySrc,
    MIN_VERSION_JOB_FILTERS,
    _filters_to_constraint,
    _parse_version,
    _to_classad_list,
)

#===============================================================================================
SCHEDD_NAME = "FILTER_SCHEDD@"
PROJECTION = ["ClusterId", "ProcId"]

#===============================================================================================
# History records written to HISTORY: Top (oldest) -> Bottom (newest)
# JID   Owner  DAGManJobId JobBatchId JobBatchName
# 1.0   alice  -           -          -
# 2.0   bob    -           "2"        "analysis"
# 2.1   bob    -           "2"        "analysis"
# 10.0  carol  -           "10"       "my.dag+10"     <- DAGMan job itself
# 11.0  carol  10          "10"       "my.dag+10"
# 12.0  carol  10          "10"       "my.dag+10"
# 12.1  carol  10          "10"       "my.dag+10"
# 20.0  dave   99          "20"       "other"
# 30.0  1abc   -           "30"       "-nightly"
# 40.0  erin   -           "40"       "back\slash"
HISTORY_RECORDS = [
    (1,  0, "alice", {}),
    (2,  0, "bob",   {"JobBatchId": "2", "JobBatchName": "analysis"}),
    (2,  1, "bob",   {"JobBatchId": "2", "JobBatchName": "analysis"}),
    (10, 0, "carol", {"JobBatchId": "10", "JobBatchName": "my.dag+10"}),
    (11, 0, "carol", {"DAGManJobId": 10, "JobBatchId": "10", "JobBatchName": "my.dag+10"}),
    (12, 0, "carol", {"DAGManJobId": 10, "JobBatchId": "10", "JobBatchName": "my.dag+10"}),
    (12, 1, "carol", {"DAGManJobId": 10, "JobBatchId": "10", "JobBatchName": "my.dag+10"}),
    (20, 0, "dave",  {"DAGManJobId": 99, "JobBatchId": "20", "JobBatchName": "other"}),
    (30, 0, "1abc",  {"JobBatchId": "30", "JobBatchName": "-nightly"}),
    (40, 0, "erin",  {"JobBatchId": "40", "JobBatchName": "back\\slash"}),
]

ALL_JOBS = {f"{c}.{p}" for c, p, _, _ in HISTORY_RECORDS}

#===============================================================================================
# Python bindings filter queries: name -> (filter kwargs, expected job ids)
# User and job id filters (passed to the history helper as non-flag job specs)
PY_JOBSPEC_FILTER_CASES = {
    "user"              : ({"users": "alice"}, {"1.0"}),
    "user_list"         : ({"users": ["alice", "dave"]}, {"1.0", "20.0"}),
    "cluster_int"       : ({"jobs": 2}, {"2.0", "2.1"}),
    "cluster_str"       : ({"jobs": "12"}, {"12.0", "12.1"}),
    "job_id"            : ({"jobs": "12.1"}, {"12.1"}),
    "job_list"          : ({"jobs": [1, "2.0"]}, {"1.0", "2.0"}),
    # Leading zeros are dropped (old ClassAd syntax rejects them)
    "cluster_zeros"     : ({"jobs": "012"}, {"12.0", "12.1"}),
    "job_id_zeros"      : ({"jobs": "012.01"}, {"12.1"}),
    # Users and job ids are OR'd together (same as condor_history)
    "user_or_job"       : ({"users": "alice", "jobs": "12.1"}, {"1.0", "12.1"}),
    # Filters are AND'd with the constraint
    "user_and_const"    : ({"users": "carol", "constraint": "ProcId == 1"}, {"12.1"}),
    "no_match_user"     : ({"users": "nobody"}, set()),
    # Leading digits do not make a username a job id
    "user_leading_digit": ({"users": "1abc"}, {"30.0"}),
}

# DAGMan job id and batch filters (passed to the history helper as -dagid/-batchid/-batchname)
PY_WORKFLOW_FILTER_CASES = {
    "dagid"             : ({"dagids": 10}, {"11.0", "12.0", "12.1"}),
    "dagid_list"        : ({"dagids": [10, 99]}, {"11.0", "12.0", "12.1", "20.0"}),
    "dagid_str"         : ({"dagids": "10"}, {"11.0", "12.0", "12.1"}),
    "dagid_mixed_list"  : ({"dagids": [10, "99"]}, {"11.0", "12.0", "12.1", "20.0"}),
    "batchid_str"       : ({"batchids": "10"}, {"10.0", "11.0", "12.0", "12.1"}),
    "batchid_int"       : ({"batchids": 2}, {"2.0", "2.1"}),
    "batchname"         : ({"batchnames": "analysis"}, {"2.0", "2.1"}),
    "batchname_dag"     : ({"batchnames": ["my.dag+10"]}, {"10.0", "11.0", "12.0", "12.1"}),
    # Different filter kinds are OR'd together (same as condor_history)
    "user_or_dagid"     : ({"users": "alice", "dagids": 99}, {"1.0", "20.0"}),
    "job_or_batchname"  : ({"jobs": 1, "batchnames": "analysis"}, {"1.0", "2.0", "2.1"}),
    # Filters are AND'd with the constraint
    "dagid_and_const"   : ({"dagids": 10, "constraint": "ProcId == 1"}, {"12.1"}),
    "no_match_batch"    : ({"batchnames": "does-not-exist"}, set()),
    # condor_history always takes the argument after -batchname as its value
    "batchname_dash"    : ({"batchnames": "-nightly"}, {"30.0"}),
    # Backslashes must survive the Python -> C glue -> schedd round trip
    "batchname_backslash": ({"batchnames": "back\\slash"}, {"40.0"}),
}

#===============================================================================================
# Python bindings argument validation: name -> (filter kwargs, expected exception type)
PY_INVALID_ARG_CASES = {
    "user_wrong_type"       : ({"users": 5}, TypeError),
    "user_list_wrong_type"  : ({"users": ["alice", 5]}, TypeError),
    "user_option"           : ({"users": "-search"}, ValueError),
    "user_looks_like_job"   : ({"users": "123"}, ValueError),
    "user_quote"            : ({"users": 'al"ice'}, ValueError),
    "user_backslash"        : ({"users": "al\\ice"}, ValueError),
    "user_control_char"     : ({"users": "al\tice"}, ValueError),
    "user_non_ascii"        : ({"users": "\u00e9lise"}, ValueError),
    "job_negative_str"      : ({"jobs": "-5"}, ValueError),
    "job_negative_int"      : ({"jobs": -1}, ValueError),
    "job_zero_int"          : ({"jobs": 0}, ValueError),
    "job_zero_str"          : ({"jobs": "0"}, ValueError),
    "job_zero_cluster"      : ({"jobs": "0.1"}, ValueError),
    "job_whitespace"        : ({"jobs": " 12"}, ValueError),
    "job_underscore"        : ({"jobs": "1_000"}, ValueError),
    "job_bad_format"        : ({"jobs": "1.2.3"}, ValueError),
    "job_unicode_digits"    : ({"jobs": "\u0661\u0662"}, ValueError),
    "job_bool"              : ({"jobs": True}, TypeError),
    "job_float"             : ({"jobs": 1.5}, TypeError),
    "dagid_bad_str"         : ({"dagids": "abc"}, ValueError),
    "dagid_job_id_str"      : ({"dagids": "10.0"}, ValueError),
    "dagid_zero_str"        : ({"dagids": "0"}, ValueError),
    "dagid_float"           : ({"dagids": 10.0}, TypeError),
    "dagid_bool"            : ({"dagids": True}, TypeError),
    "dagid_zero"            : ({"dagids": 0}, ValueError),
    "dagid_negative"        : ({"dagids": -10}, ValueError),
    "batchid_empty"         : ({"batchids": ""}, ValueError),
    "batchid_option"        : ({"batchids": "-file"}, ValueError),
    "batchid_negative"      : ({"batchids": -2}, ValueError),
    "batchid_bool"          : ({"batchids": False}, TypeError),
    "batchname_empty"       : ({"batchnames": ""}, ValueError),
    "batchname_wrong_type"  : ({"batchnames": 7}, TypeError),
}

#===============================================================================================
# Raw query filter lists sent straight to the schedd (bypassing Python validation)
# that must be rejected: name -> (filter position, ClassAd list string)
# Filter positions match the trailing _history_query arguments
JOBSPEC, DAGID, BATCHID, BATCHNAME = range(4)
SERVER_REJECT_CASES = {
    "jobspec_option"        : (JOBSPEC, '{"-search", "/etc/passwd"}'),
    "jobspec_option_only"   : (JOBSPEC, '{"-file"}'),
    "jobspec_quote"         : (JOBSPEC, '{"al\\"ice"}'),
    "jobspec_backslash"     : (JOBSPEC, '{"al\\\\ice"}'),
    "jobspec_empty"         : (JOBSPEC, '{""}'),
    "dagid_not_digits"      : (DAGID, '{"abc"}'),
    "dagid_option"          : (DAGID, '{"-1"}'),
    "batchid_option"        : (BATCHID, '{"-search"}'),
    "batchname_empty"       : (BATCHNAME, '{""}'),
    "not_a_list"            : (JOBSPEC, '"alice"'),
    "non_string_element"    : (DAGID, '{10}'),
    "non_literal_element"   : (BATCHNAME, '{strcat("ana", "lysis")}'),
}

#===============================================================================================
# condor_history remote queries: name -> (filter arguments, expected job ids, expected helper args)
CONDOR_HISTORY_CASES = {
    "user"          : (["alice"], {"1.0"}, ["alice"]),
    "cluster"       : (["12"], {"12.0", "12.1"}, ["12"]),
    "job_id"        : (["12.1"], {"12.1"}, ["12.1"]),
    "dagid"         : (["-dagid", "10"], {"11.0", "12.0", "12.1"}, ["-dagid 10"]),
    "batchid"       : (["-batchid", "2"], {"2.0", "2.1"}, ["-batchid 2"]),
    "batchname"     : (["-batchname", "analysis"], {"2.0", "2.1"}, ["-batchname analysis"]),
    "user_or_dagid" : (["alice", "-dagid", "99"], {"1.0", "20.0"}, ["alice", "-dagid 99"]),
    "user_digits"   : (["1abc"], {"30.0"}, ["1abc"]),
    "batchname_dash": (["-batchname", "-nightly"], {"30.0"}, ["-batchname -nightly"]),
    "batchname_backslash": (["-batchname", "back\\slash"], {"40.0"}, ["-batchname"]),
}

#===============================================================================================
def write_history(path):
    offset = 0
    with open(path, "w") as f:
        for i, (cluster, proc, owner, attrs) in enumerate(HISTORY_RECORDS):
            ad = classad.ClassAd({
                "ClusterId": cluster,
                "ProcId": proc,
                "Owner": owner,
                "QDate": 1000 + i,
                "CompletionDate": 1060 + i,
                "JobStatus": 4,
                "Cmd": "/bin/true",
            })
            ad.update(attrs)
            # History files use the old ClassAd syntax
            record = ad.printOld()
            record += (f"*** Offset = {offset} ClusterId = {cluster} ProcId = {proc} "
                       f"Owner = \"{owner}\" CompletionDate = {1060 + i}\n")
            f.write(record)
            offset += len(record)


def job_ids(ads):
    return {f"{ad['ClusterId']}.{ad['ProcId']}" for ad in ads}


def helper_invocations(log):
    """Return the history helper command lines logged (D_FULLDEBUG) by the schedd"""
    return [line.split("invoking", 1)[1] for line in log.splitlines() if "invoking" in line]


def has_helper_args(log, args):
    """True if a single history helper invocation contains every arg as a standalone token
    (i.e. not just inside the -constraint expression)"""
    for cmdline in helper_invocations(log):
        if all(re.search(r"(^|\s)" + re.escape(arg) + r"(\s|$)", cmdline) for arg in args):
            return True
    return False


def version_string(version):
    return "$CondorVersion: {}.{}.{} 2026-01-01 BuildID: 0 $".format(*version)


def version_before(version):
    major, minor, sub = version
    if sub > 0:
        return (major, minor, sub - 1)
    if minor > 0:
        return (major, minor - 1, 99)
    return (major - 1, 99, 99)


def fake_schedd(version):
    # No daemon is contacted: these Schedd objects are only used for checks
    # that must fail before any network communication
    location = classad.ClassAd({"MyAddress": "<127.0.0.1:9>", "CondorVersion": version})
    return htcondor.Schedd(location)


def old_schedd(schedd):
    # The real schedd, but claiming a version that predates the history filters
    location = classad.ClassAd({"MyAddress": schedd._addr,
                                "CondorVersion": version_string(version_before(MIN_VERSION_JOB_FILTERS))})
    return htcondor.Schedd(location)


def capture_history_query(monkeypatch):
    """Replace the remote history query with one that records its arguments"""
    calls = []
    def fake_history_query(*args):
        calls.append(args)
        return []
    monkeypatch.setattr(history_helper, "_history_query", fake_history_query)
    return calls


def raw_query(addr, filters):
    lists = ["", "", "", ""]
    for pos, value in filters.items():
        lists[pos] = value
    return _history_query(
        addr, "", ",".join(PROJECTION), -1, "", "",
        HistorySrc.ScheddJob, CMD_SCHEDD_HISTORY, *lists,
        True, # use_librarian
    )

#===============================================================================================
@standup
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "SCHEDD_NAME": SCHEDD_NAME,
            # Needed to verify the arguments passed to the history helper
            "SCHEDD_DEBUG": "D_FULLDEBUG",
        },
    ) as condor:
        yield condor


@action
def history_file(condor):
    p = condor.run_command(["condor_config_val", "HISTORY"])
    assert p.returncode == 0
    write_history(p.stdout.strip())
    return p.stdout.strip()


@action
def schedd(condor, history_file):
    with condor.use_config():
        return htcondor.Schedd()


class LogWatcher:
    """Read only the SchedLog text written after mark() was called"""

    def __init__(self, path):
        self.path = path
        self.offset = 0

    def mark(self):
        self.offset = self.path.stat().st_size

    def new_text(self):
        with self.path.open("r", errors="replace") as f:
            f.seek(self.offset)
            return f.read()


@action
def schedd_log(condor):
    return LogWatcher(condor.schedd_log.path)

#-----------------------------------------------------------------------------------------------
@action(params={name: name for name in PY_JOBSPEC_FILTER_CASES})
def py_jobspec_filter_case(request):
    return PY_JOBSPEC_FILTER_CASES[request.param]


@action(params={name: name for name in PY_WORKFLOW_FILTER_CASES})
def py_workflow_filter_case(request):
    return PY_WORKFLOW_FILTER_CASES[request.param]


@action(params={name: name for name in PY_INVALID_ARG_CASES})
def py_invalid_case(request):
    return PY_INVALID_ARG_CASES[request.param]


@action(params={name: name for name in SERVER_REJECT_CASES})
def server_reject_case(request):
    return SERVER_REJECT_CASES[request.param]


@action(params={name: name for name in CONDOR_HISTORY_CASES})
def condor_history_case(request):
    return CONDOR_HISTORY_CASES[request.param]

#===============================================================================================
class TestPythonFilterValidation:
    """Checks done in the Python bindings before contacting the remote daemon"""

    NEW_VERSION = version_string(MIN_VERSION_JOB_FILTERS)

    def test_invalid_filter_args(self, py_invalid_case):
        kwargs, exc = py_invalid_case
        with pytest.raises(exc):
            fake_schedd(self.NEW_VERSION).history(projection=PROJECTION, **kwargs)

    def test_invalid_filter_args_epochs(self, py_invalid_case):
        kwargs, exc = py_invalid_case
        with pytest.raises(exc):
            fake_schedd(self.NEW_VERSION).jobEpochHistory(projection=PROJECTION, **kwargs)

    @pytest.mark.parametrize("version", [
        version_string(version_before(MIN_VERSION_JOB_FILTERS)),
        version_string((9, 0, 0)),
        # Supported version number, but not the full version string form
        ".".join(str(n) for n in MIN_VERSION_JOB_FILTERS),
        "garbage",
        "",
    ], ids=["before_min", "old", "bare_number", "garbage", "empty"])
    def test_old_version_filters_sent_as_constraint(self, monkeypatch, version):
        calls = capture_history_query(monkeypatch)
        fake_schedd(version).history(projection=PROJECTION, users="alice", jobs="12.1",
                                     constraint="ProcId == 1")
        assert len(calls) == 1
        _, constraint, *_, jobspecs, dagids, batchids, batchnames, _ = calls[0]
        assert constraint == '(Owner == "alice" || (ClusterId == 12 && ProcId == 1)) && (ProcId == 1)'
        assert (jobspecs, dagids, batchids, batchnames) == ("", "", "", "")

    @pytest.mark.parametrize("since, expected", [
        (12, "ClusterID == 12"),
        ("12", "ClusterID == 12"),
        ("012.01", "ClusterID == 12 && ProcID == 1"),
        ("ProcId == 1", "ProcId == 1"),
    ])
    def test_since_job_id(self, monkeypatch, since, expected):
        calls = capture_history_query(monkeypatch)
        fake_schedd(self.NEW_VERSION).history(projection=PROJECTION, since=since)
        assert calls[0][4] == expected

    def test_since_unicode_digits(self):
        # Not a job id (ASCII digits only), nor a valid expression
        with pytest.raises(ValueError):
            fake_schedd(self.NEW_VERSION).history(projection=PROJECTION, since="\u0661\u0662")

    def test_new_version_filters_sent_as_filters(self, monkeypatch):
        calls = capture_history_query(monkeypatch)
        fake_schedd(self.NEW_VERSION).history(projection=PROJECTION, users="alice", dagids=10)
        assert len(calls) == 1
        _, constraint, *_, jobspecs, dagids, batchids, batchnames, _ = calls[0]
        assert constraint == ""
        assert (jobspecs, dagids, batchids, batchnames) == ('{"alice"}', '{"10"}', "", "")

    # The constraint is old ClassAd syntax: only quotes are escaped
    @pytest.mark.parametrize("args, expected", [
        ((None, ["alice"], [], [], [], []), 'Owner == "alice"'),
        (("", [], ["2", "12.1"], [], [], []), 'ClusterId == 2 || (ClusterId == 12 && ProcId == 1)'),
        (("ProcId == 0", [], [], ["10"], ["20"], []),
            '(DAGManJobId == 10 || JobBatchId =?= "20") && (ProcId == 0)'),
        ((classad.ExprTree("ProcId == 0"), [], [], [], [], ['back\\slash', 'a"b']),
            '(JobBatchName =?= "back\\slash" || JobBatchName =?= "a\\"b") && (ProcId == 0)'),
    ], ids=["user", "jobs", "dag_batch_and_const", "quoting_and_exprtree"])
    def test_filters_to_constraint(self, args, expected):
        assert _filters_to_constraint(*args) == expected

    @pytest.mark.parametrize("version, expected", [
        ("$CondorVersion: 26.3.0 2026-01-01 BuildID: 0 $", (26, 3, 0)),
        ("$CondorVersion: 25.0.12 2025-01-01 BuildID: 1 PackageID: 2 $", (25, 0, 12)),
        ("26.3.0", None),
        ("", None),
        (None, None),
    ])
    def test_parse_version(self, version, expected):
        assert _parse_version(version) == expected

    @pytest.mark.parametrize("items", [
        ["alice"],
        ["alice", "bob@example.org", "12", "3.4"],
        ['quoted "name"', "back\\slash", "comma, list", "{brace}"],
    ])
    def test_classad_list_round_trip(self, items):
        list_str = _to_classad_list(items)
        assert classad.ExprTree(list_str).eval() == items

    def test_empty_classad_list(self):
        assert _to_classad_list([]) == ""

#===============================================================================================
class TestScheddFilterEnforcement:
    """The schedd must reject filter values that could be interpreted as condor_history options"""

    def test_schedd_rejects_bad_filter(self, condor, schedd, server_reject_case):
        pos, value = server_reject_case
        with condor.use_config():
            with pytest.raises(HTCondorException):
                raw_query(schedd._addr, {pos: value})

    def test_schedd_rejects_without_running_helper(self, condor, schedd, schedd_log):
        schedd_log.mark()
        with condor.use_config():
            with pytest.raises(HTCondorException, match="Invalid"):
                raw_query(schedd._addr, {JOBSPEC: '{"-search", "/etc/passwd"}'})
        # The request must be rejected before the history helper is ever spawned
        assert helper_invocations(schedd_log.new_text()) == []

    def test_unfiltered_query_still_works(self, condor, schedd):
        with condor.use_config():
            ads = schedd.history(projection=PROJECTION)
        assert job_ids(ads) == ALL_JOBS

#===============================================================================================
class TestPythonJobSpecFilters:
    """End to end user and job id filtering via the Python bindings"""

    def test_history_filters(self, condor, schedd, py_jobspec_filter_case):
        kwargs, expected = py_jobspec_filter_case
        with condor.use_config():
            ads = schedd.history(projection=PROJECTION, **kwargs)
        assert job_ids(ads) == expected

    def test_old_schedd_history_filters(self, condor, schedd, py_jobspec_filter_case):
        kwargs, expected = py_jobspec_filter_case
        with condor.use_config():
            ads = old_schedd(schedd).history(projection=PROJECTION, **kwargs)
        assert job_ids(ads) == expected

    def test_filters_passed_to_helper(self, condor, schedd, schedd_log):
        schedd_log.mark()
        with condor.use_config():
            schedd.history(projection=PROJECTION, users="alice", jobs="12.1")
        assert has_helper_args(schedd_log.new_text(), ["alice", "12.1"])
        assert not has_helper_args(schedd_log.new_text(), ["-no-librarian"])

    def test_no_librarian_passed_to_helper(self, condor, schedd, schedd_log):
        schedd_log.mark()
        with condor.use_config():
            schedd.history(projection=PROJECTION, users="alice", use_librarian=False)
        assert has_helper_args(schedd_log.new_text(), ["alice", "-no-librarian"])


class TestPythonWorkflowFilters:
    """End to end DAGMan job id and batch filtering via the Python bindings"""

    def test_history_filters(self, condor, schedd, py_workflow_filter_case):
        kwargs, expected = py_workflow_filter_case
        with condor.use_config():
            ads = schedd.history(projection=PROJECTION, **kwargs)
        assert job_ids(ads) == expected

    def test_old_schedd_history_filters(self, condor, schedd, py_workflow_filter_case):
        kwargs, expected = py_workflow_filter_case
        with condor.use_config():
            ads = old_schedd(schedd).history(projection=PROJECTION, **kwargs)
        assert job_ids(ads) == expected

    def test_old_schedd_filters_not_passed_to_helper(self, condor, schedd, schedd_log):
        schedd_log.mark()
        with condor.use_config():
            old_schedd(schedd).history(projection=PROJECTION, users="alice", dagids=10)
        assert helper_invocations(schedd_log.new_text()) != []
        assert not has_helper_args(schedd_log.new_text(), ["alice"])
        assert not has_helper_args(schedd_log.new_text(), ["-dagid"])

    def test_filters_passed_to_helper(self, condor, schedd, schedd_log):
        schedd_log.mark()
        with condor.use_config():
            schedd.history(projection=PROJECTION, users="alice", dagids=10,
                           batchids="20", batchnames="analysis")
        assert has_helper_args(schedd_log.new_text(),
                               ["alice", "-dagid 10", "-batchid 20", "-batchname analysis"])

#===============================================================================================
class TestCondorHistoryRemoteFilters:
    """condor_history bundles its filters into the remote query for the helper"""

    def test_condor_history_remote_filters(self, condor, history_file, schedd_log, condor_history_case):
        args, expected, helper_args = condor_history_case
        schedd_log.mark()
        p = condor.run_command(["condor_history", "-name", SCHEDD_NAME, *args, "-af", "ClusterId", "ProcId"])
        assert p.returncode == 0
        assert p.stderr == ""
        found = {".".join(line.split()) for line in p.stdout.splitlines() if line.strip()}
        assert found == expected
        assert has_helper_args(schedd_log.new_text(), helper_args)

    def test_condor_history_remote_no_librarian(self, condor, history_file, schedd_log):
        schedd_log.mark()
        p = condor.run_command(["condor_history", "-name", SCHEDD_NAME, "-no-librarian", "alice",
                                "-af", "ClusterId", "ProcId"])
        assert p.returncode == 0
        assert has_helper_args(schedd_log.new_text(), ["alice", "-no-librarian"])

    def test_condor_history_remote_rejects_option_like_value(self, condor, history_file, schedd_log):
        # A -batchid value that looks like an option is rejected by the schedd
        # and must never reach the history helper's command line
        schedd_log.mark()
        p = condor.run_command(["condor_history", "-name", SCHEDD_NAME, "-batchid", "-file", "-af", "ClusterId"])
        assert p.returncode != 0
        assert "Invalid" in p.stdout + p.stderr
        assert helper_invocations(schedd_log.new_text()) == []
