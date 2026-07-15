import os
import re
import sys
import time

import htcondor2
import pytest

# Every condor log line is optionally prefixed with "(pid:NNN)" when D_PID
# is enabled (see debug.cpp), the same header shadow logs use -- and
# DAGMan's own dagman.out is written with the same dprintf machinery.
_PID_RE = re.compile(r"\(pid:(\d+)\)")

# Shared across every test in this suite that needs real kill -9 semantics
# (CRASH-mode restarts, or killing DAGMan itself mid-recovery) -- neither
# is meaningful on Windows.
NEEDS_KILL9 = pytest.mark.skipif(
    sys.platform not in ("linux", "darwin"), reason="needs real kill -9 semantics"
)


def submit_with_retry(condor, submit_description, timeout: int = 120):
    """
    condor.submit() can transiently fail right after a schedd restart:
    the python bindings' cached security session was established against
    the now-dead schedd instance, and the freshly-restarted one rejects
    it (HTCondorException: ... SECMAN:2004:Server rejected our session
    id). This is a known ornithology-suite quirk, not specific to these
    tests -- see test_unblocking_jobs.py's test_kill_schedd, which hits
    and works around the same thing -- so retry rather than fail outright.

    Retries for up to ``timeout`` seconds of wall clock rather than a
    fixed attempt count, so this scales with how long the schedd actually
    takes to settle on a given machine instead of guessing a count that
    might not be enough on a slow one.
    """
    deadline = time.time() + timeout
    while True:
        try:
            return condor.submit(submit_description)
        except htcondor2.HTCondorException:
            if time.time() >= deadline:
                raise
            time.sleep(2)


def get_dagman_pid(dagman_out_path, timeout: int = 120, after_pos: int = 0) -> int:
    """
    Scrape DAGMan's own pid out of its ``dagman.out`` log, mirroring the
    shadow-log pid-scraping technique used elsewhere in this test suite
    (test_unblocking_jobs.py's the_shadow_jobs fixture).

    ``dagman.out`` is appended to (not truncated) across repeated
    recovery/rescue submissions against the same dag file, so pass
    ``after_pos`` (e.g. the file's size just before submitting) to make
    sure the *newest* invocation's pid is returned instead of an earlier,
    already-finished one.
    """
    deadline = time.time() + timeout
    while True:
        if os.path.exists(dagman_out_path):
            with open(dagman_out_path, "r") as f:
                f.seek(after_pos)
                for line in f:
                    match = _PID_RE.search(line)
                    if match:
                        return int(match.group(1))
        if time.time() >= deadline:
            raise TimeoutError(
                "Could not find DAGMan's pid in {} (after byte {}) within {}s".format(
                    dagman_out_path, after_pos, timeout
                )
            )
        time.sleep(0.5)


def kill_dagman(dagman_out_path, timeout: int = 120, after_pos: int = 0) -> int:
    """
    SIGKILL the DAGMan process directly, without touching schedd/master --
    simulates DAGMan itself crashing (e.g. mid recovery-mode bootstrap),
    independent of any AP/schedd restart. Returns the pid that was killed.
    """
    if sys.platform == "win32":
        raise NotImplementedError("kill_dagman() needs real kill -9 semantics; not supported on Windows")
    pid = get_dagman_pid(dagman_out_path, timeout=timeout, after_pos=after_pos)
    os.kill(pid, 9)
    return pid


def write_attempt_script(script_path, attempts_log) -> None:
    """
    Write a python script (``attempt.py <node_name> <sleep-seconds-or-proceed-file>``)
    that appends ``"<node_name> <pid> <time>"`` to ``attempts_log`` every
    time it *starts* running, then either sleeps a fixed duration or waits
    on a "proceed" file before exiting 0, depending on its second argument:

    - If the argument parses as a number, sleep that many seconds (the
      simple case, used for nodes whose timing genuinely doesn't matter,
      e.g. the ``0``-second bookend nodes A/C).
    - Otherwise, treat it as a path and poll for that file's existence,
      exiting as soon as it appears. This lets a test control exactly
      *when* a node is allowed to finish (e.g. only after a restart has
      definitely landed) rather than guessing with a fixed sleep and
      racing the real time a restart takes on a given machine -- see
      write_recoverability_dag()'s ``long_node_proceed_file``.

    Tests use ``attempts_log`` both to count how many times a node's job
    actually ran (a completed node re-running after an interruption is a
    real regression; an interrupted node re-running is expected recovery
    behavior) and, for the long-running node, as a reliable "this node is
    now mid-run" signal to interrupt against.
    """
    with open(script_path, "w") as f:
        f.write(
            '''#!/usr/bin/env python3
import os
import sys
import time

name = sys.argv[1]
wait_arg = sys.argv[2]

with open({attempts_log!r}, "a") as log:
    log.write("{{}} {{}} {{}}\\n".format(name, os.getpid(), time.time()))

try:
    time.sleep(float(wait_arg))
except ValueError:
    proceed_file = wait_arg
    while not os.path.exists(proceed_file):
        time.sleep(0.1)
'''.format(attempts_log=str(attempts_log))
        )
    os.chmod(script_path, 0o755)


def count_attempts(attempts_log, name: str) -> int:
    """How many times has node ``name`` actually started running?"""
    if not os.path.exists(attempts_log):
        return 0
    with open(attempts_log, "r") as f:
        return sum(1 for line in f if line.split()[:1] == [name])


def wait_for_attempt(attempts_log, name: str, min_count: int = 1, timeout: int = 120) -> None:
    """Block until node ``name`` has started running at least ``min_count`` times."""
    deadline = time.time() + timeout
    while count_attempts(attempts_log, name) < min_count:
        if time.time() >= deadline:
            raise TimeoutError(
                "Node {} did not reach {} attempt(s) in {}'s within {}s".format(
                    name, min_count, attempts_log, timeout
                )
            )
        time.sleep(0.5)


def wait_for_all_attempts(attempts_log, names, timeout: int = 300) -> None:
    """
    Poll ``attempts_log`` (ground truth of what actually ran) until every
    node in ``names`` has run at least once.

    This is intentionally used instead of trusting the schedd's own
    ClusterState.all_complete for the submitted DAGMan job cluster: after a
    whole-pool CRASH, DAGMan's own scheduler-universe job can survive as an
    orphan of the now-dead schedd (it's a direct child of schedd, not
    something a daemon-level SIGKILL touches), while the *new* schedd --
    reconciling its job queue from a blank slate -- spawns a duplicate
    attempt for that same job id. That duplicate loses the dag-lock file
    race against the still-alive orphan and exits immediately, which the
    new schedd reports as "that job is done" (real, but misleading -- it's
    only the duplicate that's done), even though the real DAG is still
    being carried out, invisibly to the new schedd, by the orphan.
    """
    deadline = time.time() + timeout
    while True:
        counts = {name: count_attempts(attempts_log, name) for name in names}
        if all(count >= 1 for count in counts.values()):
            return
        if time.time() >= deadline:
            raise TimeoutError(
                "Not all of {} completed within {}s (have: {})".format(
                    names, timeout, counts
                )
            )
        time.sleep(0.5)


def write_recoverability_dag(
    dag_path, script_path, long_node_sleep: int = 30, long_node_proceed_file=None
) -> None:
    """
    Write a small A -> B -> C PARENT/CHILD DAG where every node runs
    ``write_attempt_script``'s script; B is the long-running node meant to
    be caught mid-execution and interrupted, A and C are quick bookend
    nodes used to confirm dependency ordering and no-duplicate-completion.

    If ``long_node_proceed_file`` is given, node B blocks until that file
    appears instead of sleeping ``long_node_sleep`` seconds -- see
    write_attempt_script(). This lets a test land a restart deterministically
    while B is guaranteed still running/blocked, then explicitly let B (or
    its post-interruption replacement) finish by creating the file, instead
    of picking a sleep duration long enough to outlast however long the
    restart actually takes on a given machine. ``long_node_sleep`` is
    ignored when this is given.
    """
    b_wait_arg = long_node_proceed_file if long_node_proceed_file is not None else long_node_sleep
    with open(dag_path, "w") as f:
        f.write(
            f'''
JOB A {{
    executable = {script_path}
    arguments  = "A 0"
    universe   = local
    log        = $(JOB).log
}}

JOB B {{
    executable = {script_path}
    arguments  = "B {b_wait_arg}"
    universe   = local
    log        = $(JOB).log
}}

JOB C {{
    executable = {script_path}
    arguments  = "C 0"
    universe   = local
    log        = $(JOB).log
}}

PARENT A CHILD B
PARENT B CHILD C
'''
        )
