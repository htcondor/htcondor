#!/usr/bin/env pytest

# Negotiator submitter floors and ceilings.
#
# TestLeases stands up a central-manager-only pool and exercises the
# condor_userprio lease bookkeeping for ceilings, floors and priority factors:
# seed a value, set a lease, try an illegal double-lease, cancel it, try an
# illegal double-cancel.
#
# TestEnforcement stands up a small real pool (one schedd, NUM_SLOTS static
# single-core slots, preemption off) and checks that the negotiator actually
# honors floors and ceilings when it hands out slots.  Each scenario starts
# with every slot unclaimed, queues enough held jobs for two fresh submitters,
# A and B, that either could fill the pool alone, releases both at the same
# moment, and records how the full pool ended up split between them:
#
#   even                 no floor, no ceiling            -> A=2, B=2
#   ceiling              A has a ceiling of 1            -> A=1, B=3
#   floor                A has a floor of 3              -> A=3, B=1
#
# Every scenario depends on the negotiator seeing A and B in the same cycle.
# If a cycle ran after A's submitter ad reached the collector but before B's
# did, A alone would take the pool and the split would mean nothing.  So the
# negotiator is turned off while jobs are released, and only turned back on
# once the collector shows both submitters with all their jobs idle.  (Gating
# with a negotiator knob instead would make the gate depend on the code under
# test.)  The jobs sleep far longer than the test runs and
# preemption is off, so once the pool is full the split cannot change.  Every
# wait is a poll with a generous timeout, not a fixed sleep.
#
# The "even" scenario is the control: two fresh submitters have equal
# priority, so without a floor or ceiling they split the pool evenly, and any
# other split in the remaining scenarios is the floor or ceiling at work.
#
# These pin the negotiator's behavior ahead of a refactor of how it sequences
# its bundle, floor and fair-share rounds.

import logging
import sys
import time
from typing import NamedTuple

import htcondor2 as htcondor
import pytest

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

SUBMITTER = "leaseuser@test.local"

NUM_SLOTS = 4
FLOOR = 3
# The user part of every enforcement submitter's name.
GROUP_USER = "tester"


@standup
def condor(test_dir):
    with Condor(
        test_dir / "condor",
        config={
            # Push the cycle out so the first commands aren't issued while
            # the negotiator is busy in its initial negotiation cycle.
            "DAEMON_LIST": "MASTER COLLECTOR NEGOTIATOR",
            "NEGOTIATOR_INTERVAL": "60",
        },
    ) as condor:
        yield condor


def _query_attr(condor, submitter, attr, cast):
    """Return the submitter's current value for `attr`, or None if not present."""
    rv = condor.run_command(["condor_userprio", "-allusers", "-l"])
    if rv.returncode != 0:
        return None
    current = {}
    ads = [current]
    for line in rv.stdout.splitlines():
        stripped = line.strip()
        if not stripped:
            if current:
                current = {}
                ads.append(current)
            continue
        key, _, val = stripped.partition("=")
        if not _:
            continue
        current[key.strip()] = val.strip()
    for ad in ads:
        name = ad.get("Name", "").strip('"')
        if name.lower() == submitter.lower():
            raw = ad.get(attr)
            if raw is None:
                return None
            return cast(raw)
    return None


def _wait_for_attr(condor, submitter, attr, expected, cast=int, timeout=30):
    """Poll until the submitter's `attr` matches expected, or fail."""
    deadline = time.time() + timeout
    last = None
    while time.time() < deadline:
        last = _query_attr(condor, submitter, attr, cast)
        if last == expected:
            return
        time.sleep(0.5)
    raise AssertionError(
        f"timed out waiting for {submitter} {attr} to be {expected!r}; "
        f"last value was {last!r}"
    )


def _query_ceiling(condor, submitter):
    val = _query_attr(condor, submitter, "Ceiling", int)
    return -1 if val is None else val


def _wait_for_ceiling(condor, submitter, expected, timeout=30):
    _wait_for_attr(condor, submitter, "Ceiling", expected, int, timeout)


def _run_userprio_with_retry(condor, args, timeout=30):
    """Run a condor_userprio command, retrying through transient failures
    (the negotiator may be inside a negotiation cycle and briefly
    unresponsive to command-socket connections)."""
    deadline = time.time() + timeout
    rv = None
    while time.time() < deadline:
        rv = condor.run_command(["condor_userprio"] + list(args))
        if rv.returncode == 0:
            return rv
        time.sleep(0.5)
    return rv


class TestLeases:

    def test_lease_lifecycle(self, condor):
        # Seed a baseline ceiling so the submitter exists and we have a
        # known pre-lease value to revert to. SET_CEILING is fire-and-forget
        # on the wire, so wait for it to actually take effect rather than
        # relying on a fixed sleep.
        rv = _run_userprio_with_retry(
            condor, ["-setceiling", SUBMITTER, "10"]
        )
        assert rv.returncode == 0, rv.stderr
        _wait_for_ceiling(condor, SUBMITTER, 10)

        # Install a lease; ceiling should change to the leased value.
        rv = _run_userprio_with_retry(
            condor,
            ["-setceiling", SUBMITTER, "42", "-duration", "3600"],
        )
        assert rv.returncode == 0, rv.stderr
        assert "Set ceiling lease" in rv.stdout
        _wait_for_ceiling(condor, SUBMITTER, 42)

        # A second lease on top of an in-effect lease must be rejected.
        # This command's non-zero exit is the success signal, so don't
        # retry it -- just run once.
        rv = condor.run_command(
            ["condor_userprio", "-setceiling", SUBMITTER, "99",
             "-duration", "3600"]
        )
        assert rv.returncode != 0
        assert "already in effect" in (rv.stderr + rv.stdout)
        # And the original lease value is still in effect. Use the polling
        # helper here -- condor_userprio occasionally returns exit 0 with an
        # empty body when issued back-to-back with a just-rejected command,
        # and a single-shot _query_ceiling would see that transient blip.
        _wait_for_ceiling(condor, SUBMITTER, 42)

        # Cancel restores the prior (pre-lease) ceiling.
        rv = _run_userprio_with_retry(
            condor, ["-cancelceilinglease", SUBMITTER]
        )
        assert rv.returncode == 0, rv.stderr
        _wait_for_ceiling(condor, SUBMITTER, 10)

        # Cancelling when no lease is active must be rejected.
        rv = condor.run_command(
            ["condor_userprio", "-cancelceilinglease", SUBMITTER]
        )
        assert rv.returncode != 0
        assert "no ceiling lease" in (rv.stderr + rv.stdout).lower()

    def test_floor_lease_lifecycle(self, condor):
        floor_user = "floorleaseuser@test.local"

        # Seed a baseline floor.
        rv = _run_userprio_with_retry(
            condor, ["-setfloor", floor_user, "5"]
        )
        assert rv.returncode == 0, rv.stderr
        _wait_for_attr(condor, floor_user, "Floor", 5)

        # Install a lease.
        rv = _run_userprio_with_retry(
            condor,
            ["-setfloor", floor_user, "20", "-duration", "3600"],
        )
        assert rv.returncode == 0, rv.stderr
        assert "Set floor lease" in rv.stdout
        _wait_for_attr(condor, floor_user, "Floor", 20)

        # Double-lease is rejected.
        rv = condor.run_command(
            ["condor_userprio", "-setfloor", floor_user, "30",
             "-duration", "3600"]
        )
        assert rv.returncode != 0
        assert "already in effect" in (rv.stderr + rv.stdout)
        _wait_for_attr(condor, floor_user, "Floor", 20)

        # Cancel restores the prior floor.
        rv = _run_userprio_with_retry(
            condor, ["-cancelfloorlease", floor_user]
        )
        assert rv.returncode == 0, rv.stderr
        _wait_for_attr(condor, floor_user, "Floor", 5)

        # Double-cancel is rejected.
        rv = condor.run_command(
            ["condor_userprio", "-cancelfloorlease", floor_user]
        )
        assert rv.returncode != 0
        assert "no floor lease" in (rv.stderr + rv.stdout).lower()

    def test_priority_factor_lease_lifecycle(self, condor):
        factor_user = "factorleaseuser@test.local"

        # Seed a baseline priority factor.
        rv = _run_userprio_with_retry(
            condor, ["-setfactor", factor_user, "2.0"]
        )
        assert rv.returncode == 0, rv.stderr
        _wait_for_attr(condor, factor_user, "PriorityFactor", 2.0, cast=float)

        # Install a lease.
        rv = _run_userprio_with_retry(
            condor,
            ["-setfactor", factor_user, "7.5", "-duration", "3600"],
        )
        assert rv.returncode == 0, rv.stderr
        assert "Set priority-factor lease" in rv.stdout
        _wait_for_attr(condor, factor_user, "PriorityFactor", 7.5, cast=float)

        # Double-lease is rejected.
        rv = condor.run_command(
            ["condor_userprio", "-setfactor", factor_user, "9.0",
             "-duration", "3600"]
        )
        assert rv.returncode != 0
        assert "already in effect" in (rv.stderr + rv.stdout)
        _wait_for_attr(condor, factor_user, "PriorityFactor", 7.5, cast=float)

        # Cancel restores the prior priority factor.
        rv = _run_userprio_with_retry(
            condor, ["-cancelfactorlease", factor_user]
        )
        assert rv.returncode == 0, rv.stderr
        _wait_for_attr(condor, factor_user, "PriorityFactor", 2.0, cast=float)

        # Double-cancel is rejected.
        rv = condor.run_command(
            ["condor_userprio", "-cancelfactorlease", factor_user]
        )
        assert rv.returncode != 0
        assert "no priority-factor lease" in (rv.stderr + rv.stdout).lower()


# ---------------------------------------------------------------------------
# Enforcement: does the negotiator honor floors and ceilings when matching?
# ---------------------------------------------------------------------------


@standup
def pool(test_dir):
    with Condor(
        test_dir / "pool",
        config={
            # NUM_SLOTS static single-core slots, so every slot weighs 1 and
            # floors and ceilings count slots.
            "NUM_CPUS": str(NUM_SLOTS),
            "MEMORY": str(NUM_SLOTS * 512),
            "NUM_SLOTS_TYPE_1": str(NUM_SLOTS),
            "SLOT_TYPE_1": "cpus=1",
            "SLOT_TYPE_1_PARTITIONABLE": "false",
            "NEGOTIATOR_INTERVAL": "5",
            "NEGOTIATOR_MIN_INTERVAL": "1",
            "NEGOTIATOR_CYCLE_DELAY": "1",
            "SCHEDD_INTERVAL": "5",
            "UPDATE_INTERVAL": "5",
            # With preemption off, a split, once made, stays put until the
            # jobs leave.
            "NEGOTIATOR_CONSIDER_PREEMPTION": "false",
            # Give up each claim as soon as its job leaves, so one scenario's
            # claims can't be reused by the next scenario's jobs without
            # going through the negotiator.
            "CLAIM_WORKLIFE": "0",
            # "running a floor round" is logged at D_FULLDEBUG.
            "NEGOTIATOR_DEBUG": "D_FULLDEBUG",
        },
    ) as condor:
        yield condor


class Split(NamedTuple):
    a: int
    b: int
    floor_round_ran: bool


def _submitter_name(pool, group):
    """The accountant's name for the submitter of jobs that set
    accounting_group = group and accounting_group_user = GROUP_USER."""
    with pool.use_config():
        domain = htcondor.param["ACCOUNTING_DOMAIN"]
    return f"{group}.{GROUP_USER}@{domain}"


def _set_userprio(pool, option, submitter, attr, value):
    rv = _run_userprio_with_retry(pool, [option, submitter, str(value)])
    assert rv.returncode == 0, rv.stderr
    _wait_for_attr(pool, submitter, attr, value)


def _submit_held(pool, test_dir, path_to_sleep, group, count):
    return pool.submit(
        description={
            "executable": path_to_sleep,
            # Far longer than the test, even on a slow machine: a job that
            # finishes frees a slot and changes the split.
            "arguments": "3600",
            "accounting_group": group,
            # Set explicitly: a bindings submit that sets only
            # accounting_group gets an empty user part ("group.@domain").
            "accounting_group_user": GROUP_USER,
            "hold": "true",
            "log": (test_dir / f"{group}.log").as_posix(),
            "should_transfer_files": "NO",
            "request_cpus": "1",
            "request_memory": "1",
            "request_disk": "1",
        },
        count=count,
    )


def _release_together(pool, handles):
    """Release every job in `handles` with a single action, so the schedd
    advertises all of their submitters to the negotiator in the same update."""
    constraint = " || ".join(f"ClusterId == {h.clusterid}" for h in handles)
    pool.act(htcondor.JobAction.Release, constraint)
    pool.run_command(["condor_reschedule"])


def _running_by_group(pool):
    counts = {}
    for ad in pool.query(constraint="JobStatus == 2", projection=["AccountingGroup"]):
        group = ad.get("AccountingGroup", "").split(".")[0]
        counts[group] = counts.get(group, 0) + 1
    return counts


def _wait_for_running(pool, total, timeout=180):
    """Wait until `total` jobs are running, and return them counted by group."""
    deadline = time.time() + timeout
    counts = {}
    while time.time() < deadline:
        counts = _running_by_group(pool)
        if sum(counts.values()) >= total:
            return counts
        time.sleep(1)
    raise AssertionError(
        f"timed out waiting for {total} running jobs; running by group: {counts}"
    )


def _wait_for_idle_pool(pool, timeout=180):
    """Wait until the queue is empty and every slot is unclaimed."""
    deadline = time.time() + timeout
    jobs, states = [], []
    while time.time() < deadline:
        jobs = pool.query(projection=["ClusterId", "ProcId", "JobStatus"])
        slots = pool.status(
            ad_type=htcondor.AdTypes.Startd, projection=["Name", "State"]
        )
        states = [s.get("State") for s in slots]
        if not jobs and len(states) == NUM_SLOTS and all(
            s == "Unclaimed" for s in states
        ):
            return
        time.sleep(1)
    raise AssertionError(
        f"pool did not go idle: {len(jobs)} jobs queued, slot states {states}"
    )


def _negotiator_ads(pool):
    return pool.status(ad_type=htcondor.AdTypes.Negotiator, projection=["Name"])


def _set_negotiator_running(pool, running, timeout=120):
    """Turn the negotiator off or on, and wait until it is.  The negotiator
    invalidates its collector ad when it shuts down and advertises it again
    when it starts, so the ad's presence tracks whether it can negotiate."""
    rv = pool.run_command(
        ["condor_on" if running else "condor_off", "-daemon", "negotiator"]
    )
    assert rv.returncode == 0, rv.stderr
    deadline = time.time() + timeout
    while time.time() < deadline:
        if bool(_negotiator_ads(pool)) == running:
            return
        time.sleep(0.5)
    raise AssertionError(
        f"negotiator did not turn {'on' if running else 'off'} within {timeout}s"
    )


def _wait_for_idle_submitters(pool, idle_by_submitter, timeout=120):
    """Wait until the collector's submitter ads show at least the given number
    of idle jobs for each submitter."""
    deadline = time.time() + timeout
    idle = {}
    while time.time() < deadline:
        idle = {
            ad.get("Name"): int(ad.get("IdleJobs", 0))
            for ad in pool.status(
                ad_type=htcondor.AdTypes.Submitter, projection=["Name", "IdleJobs"]
            )
        }
        if all(idle.get(n, 0) >= c for n, c in idle_by_submitter.items()):
            return
        time.sleep(1)
    raise AssertionError(
        f"submitter ads never showed {idle_by_submitter} idle; saw {idle}"
    )


def _negotiate(pool, handles, idle_by_submitter):
    """Release `handles` and let the negotiator match them, with every
    submitter in idle_by_submitter visible to the same negotiation cycle."""
    _set_negotiator_running(pool, False)
    _release_together(pool, handles)
    _wait_for_idle_submitters(pool, idle_by_submitter)
    _set_negotiator_running(pool, True)
    pool.run_command(["condor_reschedule"])


def _run_scenario(
    pool, test_dir, path_to_sleep, group_a, group_b,
    floor=0, ceiling=None,
):
    """Run one scenario to completion and return how the full pool was split
    between group_a and group_b.  Leaves the pool idle again for the next
    scenario: scenarios share one pool, so none of them may leave jobs or
    claims behind."""
    submitter_a = _submitter_name(pool, group_a)
    submitter_b = _submitter_name(pool, group_b)
    if floor:
        _set_userprio(pool, "-setfloor", submitter_a, "Floor", floor)
    if ceiling is not None:
        _set_userprio(pool, "-setceiling", submitter_a, "Ceiling", ceiling)

    _wait_for_idle_pool(pool)

    neg_log = pool.negotiator_log.open()
    for _ in neg_log.read():
        pass

    handles = []
    try:
        # Each submitter queues enough jobs to fill the pool on its own.
        held_a = _submit_held(pool, test_dir, path_to_sleep, group_a, NUM_SLOTS)
        held_b = _submit_held(pool, test_dir, path_to_sleep, group_b, NUM_SLOTS)
        handles = [held_a, held_b]
        _negotiate(
            pool, [held_a, held_b],
            {submitter_a: NUM_SLOTS, submitter_b: NUM_SLOTS},
        )

        counts = _wait_for_running(pool, NUM_SLOTS)
        for _ in neg_log.read():
            pass
        floor_round_ran = any(
            "running a floor round" in m.message for m in neg_log.messages
        )
        logger.info(
            f"scenario {group_a}/{group_b}: running by group {counts}, "
            f"floor round ran: {floor_round_ran}"
        )
        return Split(
            a=counts.get(group_a, 0),
            b=counts.get(group_b, 0),
            floor_round_ran=floor_round_ran,
        )
    finally:
        for handle in handles:
            handle.remove()
        _wait_for_idle_pool(pool)


@action
def even_split(pool, test_dir, path_to_sleep):
    return _run_scenario(pool, test_dir, path_to_sleep, "evena", "evenb")


@action
def ceiling_split(pool, test_dir, path_to_sleep):
    return _run_scenario(
        pool, test_dir, path_to_sleep, "ceila", "ceilb", ceiling=1
    )


@action
def floor_split(pool, test_dir, path_to_sleep):
    return _run_scenario(
        pool, test_dir, path_to_sleep, "floora", "floorb", floor=FLOOR
    )


@pytest.mark.skipif(
    sys.platform == "win32",
    reason="needs jobs that stay running: on the Windows CI runner the "
    "sleep.cmd polyglot exits at once, and removing running jobs between "
    "scenarios is slow and unreliable",
)
class TestEnforcement:

    def test_equal_priorities_split_evenly(self, even_split):
        # The control: if this fails, the other scenarios' expected splits
        # mean nothing.
        assert (even_split.a, even_split.b) == (NUM_SLOTS // 2, NUM_SLOTS // 2)
        assert not even_split.floor_round_ran

    def test_ceiling_caps_submitter(self, ceiling_split):
        # A stops at its ceiling; B gets the rest of the pool rather than it
        # sitting idle.
        assert (ceiling_split.a, ceiling_split.b) == (1, NUM_SLOTS - 1)

    def test_floor_round_runs(self, floor_split):
        assert floor_split.floor_round_ran

    def test_floor_guarantees_submitter(self, floor_split):
        assert (floor_split.a, floor_split.b) == (FLOOR, NUM_SLOTS - FLOOR)

