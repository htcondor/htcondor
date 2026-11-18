#!/usr/bin/env pytest

#   ====test_history_filters_librarian====
#   Verify job/user and DAG/batch history filters are answered from the
#   librarian index (direct seek) rather than a full history file scan for
#   every way of querying history:
#       - local condor_history
#       - remote condor_history (-name <schedd>)
#       - Python bindings Schedd.history()
#
#   How librarian use is verified:
#       - local condor_history: -debug output of the direct path log message
#       - remote condor_history: the history helper reports UsedLibrarian in its
#         final ad, which condor_history -debug logs
#       - Python bindings: the history helper's direct path log message in the ToolLog
#
#   Also verifies the remote history helper (always given -search) falls back to
#   a file scan when the searched file is not the librarian's indexed active file,
#   and still uses the librarian when SCHEDD.HISTORY differs from HISTORY.

from ornithology import *

import time
import sqlite3
import datetime
from pathlib import Path

import pytest

import htcondor2 as htcondor

#===============================================================================================
SCHEDD_NAME = "LIBRARIAN_SCHEDD@"
LIBRARIAN_MSG = "answering query from librarian index"
REMOTE_LIBRARIAN_MSG = "remote query was answered from the librarian index"
REMOTE_SCAN_MSG = "remote query was not answered from the librarian index"
DB_POLL_TIMEOUT = 120
PROJECTION = ["ClusterId", "ProcId"]

#===============================================================================================
# Jobs submitted (all by the test user):
#   dag_job    1 proc  batch_name=mydag     batch_id=1001 <- stand in for a DAGMan job
#   dag_node   2 procs batch_name=mydag     batch_id=1001 DAGManJobId=<dag_job>
#   batch      2 procs batch_name=analysis  batch_id=1002
#   plain      1 proc
# name -> (procs, extra submit commands)
JOBS = {
    "dag_job":  (1, {"batch_name": "mydag", "batch_id": "1001"}),
    "dag_node": (2, {"batch_name": "mydag", "batch_id": "1001"}),
    "batch":    (2, {"batch_name": "analysis", "batch_id": "1002"}),
    "plain":    (1, {}),
}
NUM_JOBS = sum(procs for procs, _ in JOBS.values())

# Query cases: name -> callable(clusters, owner) returning
#     (condor_history filter args, Python history() kwargs, expected job ids)
def _ids(clusters, *names):
    return {f"{clusters[name]}.{p}" for name in names for p in range(JOBS[name][0])}

QUERY_CASES = {
    "user": lambda c, owner: (
        [owner], {"users": owner}, _ids(c, *JOBS),
    ),
    "cluster": lambda c, owner: (
        [str(c["batch"])], {"jobs": c["batch"]}, _ids(c, "batch"),
    ),
    "job_id": lambda c, owner: (
        [f"{c['dag_node']}.1"], {"jobs": f"{c['dag_node']}.1"}, {f"{c['dag_node']}.1"},
    ),
    "dagid": lambda c, owner: (
        ["-dagid", str(c["dag_job"])], {"dagids": c["dag_job"]}, _ids(c, "dag_node"),
    ),
    "batchid": lambda c, owner: (
        ["-batchid", "1002"], {"batchids": "1002"}, _ids(c, "batch"),
    ),
    "batchname": lambda c, owner: (
        ["-batchname", "mydag"], {"batchnames": "mydag"}, _ids(c, "dag_job", "dag_node"),
    ),
    "mixed": lambda c, owner: (
        [str(c["plain"]), "-batchname", "analysis"], {"jobs": c["plain"], "batchnames": "analysis"},
        _ids(c, "plain", "batch"),
    ),
}

#===============================================================================================
def job_ids(ads):
    return {f"{ad['ClusterId']}.{ad['ProcId']}" for ad in ads}


def run_history(condor, args):
    """Run condor_history with debug output; return (job ids, stderr)"""
    p = condor.run_command(["condor_history", "-debug:D_FULLDEBUG", *args, "-af", *PROJECTION])
    assert p.returncode == 0, p.stderr
    found = {".".join(line.split()) for line in p.stdout.splitlines() if line.strip()}
    return found, p.stderr


def run_remote_history(condor, args):
    return run_history(condor, ["-name", SCHEDD_NAME, *args])


def python_history(condor, tool_log, **kwargs):
    """Query history via the Python bindings; return (job ids, whether the
    history helper logged answering from the librarian)"""
    for _ in tool_log.read(): # Skip messages from earlier queries
        pass
    with condor.use_config():
        ads = htcondor.Schedd().history(projection=PROJECTION, **kwargs)
    return job_ids(ads), any(LIBRARIAN_MSG in msg.message for msg in tool_log.read())

#===============================================================================================
@standup
def condor(test_dir):
    (test_dir / "config.d").mkdir(exist_ok=True)
    with Condor(
        local_dir=test_dir / "condor",
        raw_config="use feature : Librarian",
        config={
            "LIBRARIAN_UPDATE_INTERVAL": 2,
            "SCHEDD_NAME": SCHEDD_NAME,
            # The remote history helper (condor_history -inherit) writes to the ToolLog
            "TOOL_LOG": "$(LOG)/ToolLog",
            "TOOL_DEBUG": "D_FULLDEBUG",
            # The history helper is a new process for each query, so it reads
            # config files dropped in here without a reconfig
            "LOCAL_CONFIG_DIR": str(test_dir / "config.d"),
        },
    ) as condor:
        yield condor


@action
def params(condor):
    with condor.use_config():
        return {knob: htcondor.param[knob] for knob in ("HISTORY", "LIBRARIAN_DATABASE", "LIBRARIAN_LOG", "TOOL_LOG")}


@action
def clusters(condor, path_to_sleep, test_dir, params):
    # Submit everything first so the jobs run concurrently, then wait for all of them
    handles = {}
    for name, (procs, extra) in JOBS.items():
        description = {
            "executable": path_to_sleep,
            "arguments": "0",
            "log": str(test_dir / f"{name}.log"),
            **extra,
        }
        if name == "dag_node":
            description["MY.DAGManJobId"] = str(handles["dag_job"].clusterid)
        handles[name] = condor.submit(description=description, count=procs)

    for name, handle in handles.items():
        assert handle.wait(
            condition=ClusterState.all_complete,
            fail_condition=ClusterState.any_held,
            timeout=120,
            verbose=True,
        ), f"{name} jobs did not complete"

    # Wait for the librarian to index every job record
    start = time.time()
    count = 0
    while time.time() - start < DB_POLL_TIMEOUT:
        try:
            with sqlite3.connect(params["LIBRARIAN_DATABASE"]) as conn:
                count = conn.execute("SELECT COUNT(*) FROM JobRecords").fetchone()[0]
        except sqlite3.OperationalError:
            count = 0
        if count >= NUM_JOBS:
            return {name: handle.clusterid for name, handle in handles.items()}
        time.sleep(1)
    pytest.fail(f"Librarian indexed {count} of {NUM_JOBS} job records within {DB_POLL_TIMEOUT}s")


@action
def owner(condor, clusters):
    with condor.use_config():
        # Plain constraint (not the filters under test) so setup can't fail because of them
        ads = htcondor.Schedd().history(constraint=f"ClusterId == {clusters['plain']}", projection=["Owner"], match=1)
    return ads[0]["Owner"]


@action
def tool_log(params):
    path = Path(params["TOOL_LOG"])
    path.touch() # No tool may have logged yet
    return DaemonLog(path).open()


@action(params={name: name for name in QUERY_CASES})
def query_case(request, clusters, owner):
    return QUERY_CASES[request.param](clusters, owner)

#===============================================================================================
class TestLocalCondorHistoryLibrarian:

    def test_local_query_uses_librarian(self, condor, query_case):
        args, _, expected = query_case
        found, debug = run_history(condor, args)
        assert found == expected
        assert LIBRARIAN_MSG in debug

    def test_local_query_matches_file_scan(self, condor, query_case):
        args, _, expected = query_case
        found, debug = run_history(condor, ["-no-librarian", *args])
        assert found == expected
        # Sanity check the log message is specific to the librarian path
        assert LIBRARIAN_MSG not in debug


class TestRemoteCondorHistoryLibrarian:

    def test_remote_query_uses_librarian(self, condor, query_case):
        args, _, expected = query_case
        found, debug = run_remote_history(condor, args)
        assert found == expected
        assert REMOTE_LIBRARIAN_MSG in debug

    def test_remote_unfiltered_query_scans_files(self, condor, clusters):
        found, debug = run_remote_history(condor, [])
        assert len(found) == NUM_JOBS
        assert REMOTE_SCAN_MSG in debug


class TestPythonHistoryLibrarian:

    def test_python_query_uses_librarian(self, condor, tool_log, query_case):
        _, kwargs, expected = query_case
        found, used_librarian = python_history(condor, tool_log, **kwargs)
        assert found == expected
        assert used_librarian

    def test_python_unfiltered_query_scans_files(self, condor, tool_log, clusters):
        # Without filters there is nothing to look up in the librarian index
        found, used_librarian = python_history(condor, tool_log)
        assert len(found) == NUM_JOBS
        assert not used_librarian


#===============================================================================================
# NOTE: These change the pool's configuration and history files, so they must stay last
class TestRemoteLibrarianArchiveMatching:

    def test_schedd_history_override_uses_librarian(self, condor, test_dir, params, clusters):
        """The helper's -search path comes from SCHEDD.HISTORY while the tool's HISTORY
        is empty: the librarian's indexed file, not HISTORY, decides librarian use.
        SCHEDD.HISTORY is the schedd's current HISTORY, so the schedd needs no reconfig.
        The override also moves TOOL_LOG, so the librarian message only shows up in the
        new log if the history helper actually read the override config."""
        override_log = test_dir / "OverrideToolLog"
        override_log.touch()
        override_stream = DaemonLog(override_log).open()

        config_file = test_dir / "config.d" / "history_override.config"
        config_file.write_text(f"SCHEDD.HISTORY = {params['HISTORY']}\nHISTORY =\nTOOL_LOG = {override_log}\n")
        try:
            args, kwargs, expected = QUERY_CASES["dagid"](clusters, None)
            found, debug = run_remote_history(condor, args)
            assert found == expected
            assert REMOTE_LIBRARIAN_MSG in debug

            found, used_librarian = python_history(condor, override_stream, **kwargs)
            assert found == expected
            assert used_librarian
        finally:
            config_file.unlink()

    def test_rotated_since_librarian_update_scans_files(self, condor, params, clusters, owner):
        """HISTORY rotated after the librarian's last update: the indexed active file's
        inode no longer matches, so the helper must scan files (and still find everything)."""
        # Stop the librarian so it can't index the rotation (this is the only time it exits)
        with condor.use_config():
            master = htcondor.Collector().locate(htcondor.DaemonType.Master)
            htcondor.send_command(master, htcondor.DaemonCommand.DaemonOff, "LIBRARIAN")
        librarian_log = DaemonLog(Path(params["LIBRARIAN_LOG"])).open()
        assert librarian_log.wait(condition=lambda msg: "EXITING WITH STATUS" in msg.message, timeout=60), \
            "Librarian did not exit"

        history = Path(params["HISTORY"])
        rotated = history.parent / f"{history.name}.{datetime.datetime.now().strftime('%Y%m%dT%H%M%S')}"
        history.rename(rotated)
        history.touch()  # New active file (new inode) the librarian has not seen

        args, _, expected = QUERY_CASES["user"](clusters, owner)
        found, debug = run_remote_history(condor, args)
        assert found == expected
        assert REMOTE_SCAN_MSG in debug
