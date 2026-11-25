#!/usr/bin/env pytest

"""
Regression tests for condor_adstash run_workers process management.
Exercises bounded concurrency, per-worker timeout enforcement,
and SIGTERM/SIGKILL escalation logic.
"""

import sys
import multiprocessing
from pathlib import Path

from libadstash import worker_fast, worker_slow, worker_hangs, worker_errors

# Add the adstash package to the path
# Try the source tree layout first, then fall back to the installed libexec location
_source_path = Path(__file__).parent.parent / "condor_scripts"
if (_source_path / "adstash").is_dir():
    sys.path.insert(0, str(_source_path))
else:
    import htcondor2 as htcondor
    sys.path.insert(0, htcondor.param.get("libexec", "/usr/libexec/condor"))

from adstash.main import run_workers


MAX_WORKERS = 2
SOFT_TIMEOUT = 10
HARD_TIMEOUT = 5


class TestAllSucceed:
    """All workers complete before the timeout."""

    def test_all_succeed(self):
        procs = [
            ("fast-a", multiprocessing.Process(target=worker_fast)),
            ("fast-b", multiprocessing.Process(target=worker_fast)),
        ]
        results = run_workers(procs, max_workers=MAX_WORKERS, soft_timeout=SOFT_TIMEOUT, hard_timeout=HARD_TIMEOUT)
        assert results["total"] == 2
        assert set(results["succeeded"]) == {"fast-a", "fast-b"}
        assert results["timed_out"] == []
        assert results["errored"] == []
        assert results["leaked_pids"] == []


class TestErrorDetected:
    """Workers that raise an exception are reported as errored."""

    def test_error_detected(self):
        procs = [
            ("error", multiprocessing.Process(target=worker_errors)),
        ]
        results = run_workers(procs, max_workers=MAX_WORKERS, soft_timeout=SOFT_TIMEOUT, hard_timeout=HARD_TIMEOUT)
        assert results["errored"] == ["error"]
        assert results["succeeded"] == []


class TestSlowWorkerSigtermed:
    """Workers that exceed soft_timeout are sent SIGTERM."""

    def test_slow_worker_sigtermed(self):
        procs = [
            ("slow", multiprocessing.Process(target=worker_slow)),
        ]
        results = run_workers(procs, max_workers=MAX_WORKERS, soft_timeout=SOFT_TIMEOUT, hard_timeout=HARD_TIMEOUT)
        assert "slow" in results["timed_out"]
        assert results["succeeded"] == []


class TestHungWorkerSigkilled:
    """Workers that ignore SIGTERM are escalated to SIGKILL after hard_timeout."""

    def test_hung_worker_sigkilled(self):
        procs = [
            ("hang", multiprocessing.Process(target=worker_hangs)),
        ]
        results = run_workers(procs, max_workers=MAX_WORKERS, soft_timeout=SOFT_TIMEOUT, hard_timeout=HARD_TIMEOUT)
        assert "hang" in results["timed_out"]
        assert "hang" in results["killed"]
        assert results["succeeded"] == []


class TestMixedBehaviors:
    """Mixed worker behaviors with bounded concurrency."""

    def test_mixed_behaviors(self):
        procs = [
            ("fast", multiprocessing.Process(target=worker_fast)),
            ("slow", multiprocessing.Process(target=worker_slow)),
            ("error", multiprocessing.Process(target=worker_errors)),
            ("hang", multiprocessing.Process(target=worker_hangs)),
        ]
        results = run_workers(procs, max_workers=MAX_WORKERS, soft_timeout=SOFT_TIMEOUT, hard_timeout=HARD_TIMEOUT)
        assert results["total"] == 4
        assert results["succeeded"] == ["fast"]
        assert "slow" in results["timed_out"]
        assert "hang" in results["timed_out"]
        assert "hang" in results["killed"]
        assert results["errored"] == ["error"]
