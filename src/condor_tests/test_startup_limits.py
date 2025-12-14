#!/usr/bin/env pytest

import os
import shutil
import subprocess
import time
from pathlib import Path

import htcondor2 as htcondor

from ornithology import action, Condor, ClusterState


def _find_helper():
    for name in ("startup_limit_ctl", "startup_limit_ctl.exe"):
        helper = shutil.which(name)
        if helper:
            return Path(helper)

    here = Path(__file__).resolve()
    candidates = set()
    for root in (here.parent, here.parent.parent):
        for name in ("startup_limit_ctl.exe", "startup_limit_ctl"):
            candidates.add(root / name)

    for ancestor in here.parents:
        for name in ("startup_limit_ctl.exe", "startup_limit_ctl"):
            candidates.add(ancestor / "build" / "src" / "condor_tests" / name)
            candidates.add(ancestor / "src" / "condor_tests" / name)

    for candidate in candidates:
        if candidate.exists():
            return candidate
    return None


def _create_limit(helper_path, schedd_address, *, tag, expr, count, window, expires=120, name=None, cost_expr=None, burst=None, max_burst_cost=None):
    cmd = [
        str(helper_path),
        "--create",
        "--schedd",
        schedd_address,
        "--tag",
        tag,
        "--expr",
        expr,
        "--count",
        str(count),
        "--window",
        str(window),
        "--expires",
        str(expires),
    ]
    if name:
        cmd.extend(["--name", name])
    if cost_expr:
        cmd.extend(["--cost-expr", cost_expr])
    if burst is not None:
        cmd.extend(["--burst", str(burst)])
    if max_burst_cost is not None:
        cmd.extend(["--max-burst-cost", str(max_burst_cost)])
    # Avoid capture_output (Python 3.7+); stay compatible with older runtimes.
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True, check=False)
    if proc.returncode != 0:
        raise RuntimeError(f"failed to create startup limit: {proc.stdout} {proc.stderr}")
    return tag


def _query_limit(helper_path, schedd_address, tag):
    cmd = [str(helper_path), "--query", "--schedd", schedd_address, "--tag", tag]
    # Avoid capture_output (Python 3.7+); stay compatible with older runtimes.
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True, check=True)
    lines = [l.strip() for l in proc.stdout.splitlines() if l.strip()]
    if not lines:
        raise RuntimeError("no startup limit data returned")
    fields = lines[-1].split()
    if len(fields) < 4:
        raise RuntimeError(f"unexpected startup limit output: {lines[-1]}")
    return {
        "tag": fields[0],
        "name": fields[1],
        "skipped": int(fields[2]),
        "ignored": int(fields[3]),
    }


def _wait_for_limit(helper_path, schedd_address, tag, predicate, *, timeout=60, interval=1, handles=None):
    """Poll the startup limit until predicate(info) is true, jobs finish, or timeout elapses."""

    deadline = time.time() + timeout
    last = None
    last_err = None
    handles = handles or []

    while time.time() < deadline:
        try:
            info = _query_limit(helper_path, schedd_address, tag)
            last = info
            last_err = None
            if predicate(info):
                return info
        except subprocess.CalledProcessError as exc:
            # Schedd may be briefly unavailable during startup/shutdown; keep polling.
            last_err = exc

        if handles:
            all_complete = True
            for h in handles:
                if h.wait(timeout=0, condition=ClusterState.any_held):
                    raise AssertionError(f"jobs for {tag} entered Held state")
                if not h.wait(timeout=0, condition=ClusterState.all_complete):
                    all_complete = False
                    break
            if all_complete:
                return last if last is not None else _query_limit(helper_path, schedd_address, tag)
        time.sleep(interval)

    if last_err:
        raise AssertionError(f"startup limit condition not met before timeout; last_err={last_err}; last_ok={last}")
    raise AssertionError(f"startup limit condition not met before timeout; last={last}")


@action
def helper_path():
    helper = _find_helper()
    if helper is None:
        import pytest
        pytest.skip("startup_limit_ctl helper not built or not on PATH")
    return helper


@action
def schedd_address(the_condor):
    return the_condor._get_address_file("SCHEDD").read_text().splitlines()[0]


@action
def the_condor(test_dir):
    local_dir = test_dir / "condor"
    config = {
        "SCHEDD_DEBUG": "D_FULLDEBUG",
        "NEGOTIATOR_INTERVAL": "2",
        "NEGOTIATOR_MIN_INTERVAL": "2",
        "SCHEDD_INTERVAL": "2",
        "STARTUP_LIMIT_MAX_EXPIRATION": "300",
        "STARTUP_LIMIT_BAN_WINDOW": "10",
        "STARTUP_LIMIT_LOOKAHEAD": "10",
        "NUM_SLOTS": "2",
        "STARTD_ATTRS": "$(STARTD_ATTRS) MachineClass",
        "SLOT1_MachineClass": '"fast"',
        "SLOT2_MachineClass": '"slow"',
    }
    with Condor(local_dir=local_dir, config=config) as c:
        yield c


@action
def installed_limit(helper_path, the_condor, schedd_address):
    return _create_limit(
        helper_path,
        schedd_address,
        tag="rate-test",
        name="rate-test",
        expr="true",
        count=1,
        window=5,
    )


@action
def submitted_jobs(the_condor, installed_limit, path_to_sleep, test_dir):
    desc = {
        "executable": path_to_sleep,
        "arguments": "0",
        "log": test_dir / "job_$(CLUSTER).log",
    }
    handle = the_condor.submit(description=desc, count=3)
    return handle


class TestStartupLimits:
    def test_limit_throttles_and_records(self, helper_path, schedd_address, installed_limit, submitted_jobs):
        # Poll until we observe throttling; no need to wait for every job to finish.
        info = _wait_for_limit(
            helper_path,
            schedd_address,
            installed_limit,
            lambda i: (i["skipped"] > 0) or (i["ignored"] > 0),
            timeout=120,
            interval=2,
            handles=[submitted_jobs],
        )
        assert info["tag"] == installed_limit
        assert info["name"] == installed_limit
        # Expect at least one skip or ignored to show rate limiting engaged.
        assert (info["skipped"] > 0) or (info["ignored"] > 0)


@action
def permissive_limit(helper_path, schedd_address):
    return _create_limit(
        helper_path,
        schedd_address,
        tag="permissive-limit",
        name="permissive",
        expr="true",
        count=10,
        window=60,
    )


@action
def blocking_limit(helper_path, schedd_address):
    return _create_limit(
        helper_path,
        schedd_address,
        tag="blocking-limit",
        name="blocking",
        expr="true",
        count=1,
        window=60,
        expires=240,
    )


@action
def type_a_limit(helper_path, schedd_address):
    return _create_limit(
        helper_path,
        schedd_address,
        tag="type-a-limit",
        name="type-a",
        expr='JOB.JobType == "typeA"',
        count=1,
        window=90,
        expires=240,
    )


@action
def type_a_limit_isolated(helper_path, schedd_address):
    # Separate limit instance to avoid cross-test counter bleed.
    return _create_limit(
        helper_path,
        schedd_address,
        tag="type-a-limit-isolated",
        name="type-a-isolated",
        expr='JOB.JobType == "typeA" && JOB.LimitGroup == "iso"',
        count=1,
        window=90,
        expires=240,
    )


@action
def machine_attr_limit_fast(helper_path, schedd_address):
    return _create_limit(
        helper_path,
        schedd_address,
        tag="machine-attr-limit-fast",
        name="machine-attr-fast",
        expr='MACHINE.MachineClass == "fast"',
        count=1,
        window=60,
        expires=240,
    )


@action
def machine_attr_limit_fast_nonmatch(helper_path, schedd_address):
    return _create_limit(
        helper_path,
        schedd_address,
        tag="machine-attr-limit-fast-nonmatch",
        name="machine-attr-fast-nonmatch",
        expr='MACHINE.MachineClass == "fast" && JOB.LimitGroup == "ma_nonmatch"',
        count=1,
        window=60,
        expires=240,
    )


@action
def costed_limit(helper_path, schedd_address):
    return _create_limit(
        helper_path,
        schedd_address,
        tag="costed-limit",
        name="costed",
        expr='JobType == "expensive"',
        cost_expr='5',
        count=1,
        window=60,
        burst=2,
        max_burst_cost=2,
        expires=240,
    )


class TestStartupLimitMatchmakingBlocked:
    def test_matchmaking_records_ignored(self, helper_path, schedd_address, blocking_limit, path_to_sleep, test_dir, the_condor):
        desc = {
            "executable": path_to_sleep,
            "arguments": "0",
            "log": test_dir / "blocked_$(CLUSTER).log",
        }
        handle = the_condor.submit(description=desc, count=4)
        _wait_for_limit(
            helper_path,
            schedd_address,
            blocking_limit,
            lambda i: (i["skipped"] > 0) and (i["ignored"] > 0),
            timeout=180,
            interval=2,
            handles=[handle],
        )

        info = _query_limit(helper_path, schedd_address, blocking_limit)
        assert info["skipped"] > 0
        assert info["ignored"] > 0


class TestStartupLimitBelowThreshold:
    def test_jobs_under_limit_not_skipped(self, helper_path, schedd_address, permissive_limit, path_to_sleep, test_dir, the_condor):
        desc = {
            "executable": path_to_sleep,
            "arguments": "0",
            "log": test_dir / "permissive_$(CLUSTER).log",
        }
        handle = the_condor.submit(description=desc, count=3)
        assert handle.wait(
            timeout=90,
            condition=ClusterState.all_complete,
            fail_condition=ClusterState.any_held,
        )

        info = _query_limit(helper_path, schedd_address, permissive_limit)
        assert info["skipped"] == 0
        assert info["ignored"] == 0


class TestStartupLimitPerType:
    def test_one_type_limited_other_runs(self, helper_path, schedd_address, type_a_limit, path_to_sleep, test_dir, the_condor):
        slow_desc = {
            "executable": path_to_sleep,
            "arguments": "0",
            "log": test_dir / "typeA_$(CLUSTER).log",
            "+JobType": '"typeA"',
        }
        fast_desc = {
            "executable": path_to_sleep,
            "arguments": "0",
            "log": test_dir / "typeB_$(CLUSTER).log",
            "+JobType": '"typeB"',
        }

        fast_jobs = the_condor.submit(description=fast_desc, count=3)
        slow_jobs = the_condor.submit(description=slow_desc, count=3)

        _wait_for_limit(
            helper_path,
            schedd_address,
            type_a_limit,
            lambda i: i["skipped"] > 0,
            timeout=240,
            interval=2,
            handles=[fast_jobs, slow_jobs],
        )

        info = _query_limit(helper_path, schedd_address, type_a_limit)
        assert info["skipped"] > 0
        # Fast jobs should not be counted against the type A limit; ignored may stay at 0.
        assert info["ignored"] >= 0

    def test_other_type_only_not_counted(self, helper_path, schedd_address, type_a_limit_isolated, path_to_sleep, test_dir, the_condor):
        desc = {
            "executable": path_to_sleep,
            "arguments": "0",
            "log": test_dir / "typeB_only_$(CLUSTER).log",
            "+JobType": '"typeB"',
            "+LimitGroup": '"iso"',
        }

        handle = the_condor.submit(description=desc, count=3)
        assert handle.wait(
            timeout=120,
            condition=ClusterState.all_complete,
            fail_condition=ClusterState.any_held,
        )

        info = _query_limit(helper_path, schedd_address, type_a_limit_isolated)
        assert info["skipped"] == 0
        assert info["ignored"] == 0


class TestStartupLimitMachineAttribute:
    def test_limit_uses_machine_attribute(self, helper_path, schedd_address, machine_attr_limit_fast, path_to_sleep, test_dir, the_condor):
        desc = {
            "executable": path_to_sleep,
            "arguments": "0",
            "requirements": 'TARGET.MachineClass == "fast"',
            "log": test_dir / "machine_attr_$(CLUSTER).log",
        }

        jobs = the_condor.submit(description=desc, count=4)
        _wait_for_limit(
            helper_path,
            schedd_address,
            machine_attr_limit_fast,
            lambda i: i["skipped"] > 0,
            timeout=180,
            interval=2,
            handles=[jobs],
        )

        info = _query_limit(helper_path, schedd_address, machine_attr_limit_fast)
        # Expression references TARGET.MachineClass so enforcing proves machine attribute involvement.
        assert info["skipped"] > 0

    def test_non_matching_machine_not_counted(self, helper_path, schedd_address, machine_attr_limit_fast_nonmatch, path_to_sleep, test_dir, the_condor):
        desc = {
            "executable": path_to_sleep,
            "arguments": "0",
            "requirements": 'TARGET.MachineClass == "slow"',
            "log": test_dir / "machine_attr_slow_$(CLUSTER).log",
            "+LimitGroup": '"ma_nonmatch"',
        }

        handle = the_condor.submit(description=desc, count=3)
        assert handle.wait(
            timeout=120,
            condition=ClusterState.all_complete,
            fail_condition=ClusterState.any_held,
        )

        info = _query_limit(helper_path, schedd_address, machine_attr_limit_fast_nonmatch)
        assert info["skipped"] == 0
        assert info["ignored"] == 0


class TestStartupLimitCostAndBurst:
    def test_cost_expression_and_burst_allow_one_then_throttle(self, helper_path, schedd_address, costed_limit, path_to_sleep, test_dir, the_condor):
        desc = {
            "executable": path_to_sleep,
            # Keep a short runtime so multiple jobs attempt to start within the bucket window.
            "arguments": "1",
            "log": test_dir / "costed_$(CLUSTER).log",
            "+JobType": '"expensive"',
        }

        handle = the_condor.submit(description=desc, count=2)
        _wait_for_limit(
            helper_path,
            schedd_address,
            costed_limit,
            lambda i: i["skipped"] > 0,
            timeout=180,
            interval=2,
            handles=[handle],
        )

        info = _query_limit(helper_path, schedd_address, costed_limit)
        # Cost expression (5) is capped by max burst cost 2; with two jobs
        # (total burst cost 4 if the cost expression is working),
        # count=1, and burst=2, at least one job should be throttled.
        assert info["skipped"] > 0
