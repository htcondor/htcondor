#!/usr/bin/env pytest

# The negotiator's priority round (NEGOTIATOR_PRIORITY_SUBMITTER_CONSTRAINT).
#
# One pool with two schedds -- the regular one and a second one named
# "favored" -- NUM_SLOTS static single-core slots, and preemption off.  Each
# scenario starts with every slot unclaimed, queues enough held jobs for two
# fresh submitters that either could fill the pool alone (O on the regular
# schedd, F on the favored one), releases both, and records how the full pool
# ended up split between them:
#
#   control              no priority round                     -> F=2, O=2
#   whole pool           priority round for F, every slot      -> F=4, O=0
#   slot subset          priority round for F, only slots 1-3
#                        (NEGOTIATOR_PRIORITY_SLOT_CONSTRAINT)  -> F=3, O=1
#   subset pie           F and G favored, only slots 1-2       -> F and G get
#                                                                 one each
#   reclaim              F favored on slots 1-3 holds all 4; O arrives and
#                        F's job on slot 2 is removed          -> F gets slot 2
#
# The control shows that without the round two fresh submitters split the
# pool evenly, so any other split is the round at work.
#
# With a slot constraint, the round matches only the slots it allows, and the
# round's pie is the weight of those slots.  In the subset-pie scenario, a
# pie of the whole pool (4) would give F and G a share of 2 each, so whichever
# went first could take both of the round's slots; a pie of the 2 slots gives
# each a share of 1.  Who gets slots 3 and 4 in the fair-share round
# afterwards depends on tie-breaking, so that scenario checks only who holds
# the round's slots.
#
# In the priority round, only what a submitter holds on the round's slots
# counts against its share of them.  In the reclaim scenario F holds slots
# 1, 3 and 4 when slot 2 frees up.  Counting slot 4 too, F's usage (3) would
# use up its share of the round's 3 slots, and the fair-share round would give
# slot 2 to O.  Counting only the round's slots, F holds 2 of its 3.
#
# The knobs change between scenarios, so each scenario appends its settings to
# the config file while the negotiator is off; condor_on starts a fresh
# negotiator, which reads them.  Turning the negotiator off while jobs are
# released also ensures it sees both submitters in the same cycle (see
# test_floor_ceiling for why that matters).

import logging
import re
import sys
import time
from typing import NamedTuple, Optional

import htcondor2 as htcondor
import pytest

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

NUM_SLOTS = 4
GROUP_USER = "tester"
FAVORED = "favored"
ROUND_RAN = re.compile(r"Negotiating for \d+ priority submitter\(s\)")
ROUND_MATCHED = re.compile(r"Priority round finished: (\d+) slot\(s\)")


@standup
def pool(test_dir):
    favored_spool = test_dir / "favored_spool"
    favored_spool.mkdir(parents=True, exist_ok=True)
    with Condor(
        test_dir / "pool",
        config={
            "DAEMON_LIST": f"MASTER COLLECTOR NEGOTIATOR STARTD SCHEDD {FAVORED.upper()}",
            # A second schedd, named "favored", run by the same master.
            FAVORED.upper(): "$(SCHEDD)",
            f"{FAVORED.upper()}_ARGS": f"-local-name {FAVORED}",
            f"{FAVORED.upper()}_LOG": "$(LOG)/SchedLog.favored",
            "DC_DAEMON_LIST": f"+ {FAVORED.upper()}",
            f"{FAVORED.upper()}.SCHEDD_NAME": FAVORED,
            f"{FAVORED.upper()}.SCHEDD_LOG": f"$({FAVORED.upper()}_LOG)",
            f"{FAVORED.upper()}.SPOOL": favored_spool.as_posix(),
            # NUM_SLOTS static single-core slots, so every slot weighs 1 and
            # the pie counts slots.
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
            "NEGOTIATOR_DEBUG": "D_FULLDEBUG",
        },
    ) as condor:
        yield condor


def _favored_schedd_ad(pool, timeout=120):
    deadline = time.time() + timeout
    while time.time() < deadline:
        ads = pool.status(
            ad_type=htcondor.AdTypes.Schedd,
            constraint=f'regexp("^{FAVORED}@", Name)',
        )
        if ads:
            return ads[0]
        time.sleep(1)
    raise AssertionError(f"the {FAVORED} schedd never advertised itself")


@action
def favored_schedd_ad(pool):
    return _favored_schedd_ad(pool)


@action
def schedds(pool, favored_schedd_ad):
    """The regular schedd, then the favored one."""
    with pool.use_config():
        return pool.get_local_schedd(), htcondor.Schedd(favored_schedd_ad)


def _submitter_name(pool, group):
    with pool.use_config():
        domain = htcondor.param["ACCOUNTING_DOMAIN"]
    return f"{group}.{GROUP_USER}@{domain}"


def _submit_held(pool, schedd, test_dir, path_to_sleep, group, count):
    sub = htcondor.Submit({
        "executable": path_to_sleep,
        # Far longer than the test, even on a slow machine: a job that
        # finishes frees a slot and changes the split.
        "arguments": "3600",
        "accounting_group": group,
        # Set explicitly: a bindings submit that sets only accounting_group
        # gets an empty user part ("group.@domain").
        "accounting_group_user": GROUP_USER,
        "hold": "true",
        "log": (test_dir / f"{group}.log").as_posix(),
        "should_transfer_files": "NO",
        "request_cpus": "1",
        "request_memory": "1",
        "request_disk": "1",
    })
    with pool.use_config():
        return schedd.submit(sub, count=count).cluster()


def _running_by_group(pool, schedds):
    counts = {}
    with pool.use_config():
        for schedd in schedds:
            for ad in schedd.query(constraint="JobStatus == 2", projection=["AccountingGroup"]):
                group = ad.get("AccountingGroup", "").split(".")[0]
                counts[group] = counts.get(group, 0) + 1
    return counts


def _slots_by_group(pool, schedds):
    """The slot names (slot1, ...) each group's running jobs are on."""
    slots = {}
    with pool.use_config():
        for schedd in schedds:
            for ad in schedd.query(constraint="JobStatus == 2",
                                   projection=["AccountingGroup", "RemoteHost"]):
                group = ad.get("AccountingGroup", "").split(".")[0]
                slots.setdefault(group, []).append(ad.get("RemoteHost", "").split("@")[0])
    return {group: sorted(names) for group, names in slots.items()}


def _wait_for_running(pool, schedds, total, timeout=180):
    deadline = time.time() + timeout
    counts = {}
    while time.time() < deadline:
        counts = _running_by_group(pool, schedds)
        if sum(counts.values()) >= total:
            return counts
        time.sleep(1)
    raise AssertionError(
        f"timed out waiting for {total} running jobs; running by group: {counts}"
    )


def _wait_for_idle_pool(pool, schedds, timeout=180):
    """Wait until both queues are empty and every slot is unclaimed."""
    deadline = time.time() + timeout
    jobs, states = [], []
    while time.time() < deadline:
        with pool.use_config():
            jobs = [ad for s in schedds for ad in s.query(projection=["ClusterId"])]
        slots = pool.status(ad_type=htcondor.AdTypes.Startd, projection=["State"])
        states = [s.get("State") for s in slots]
        if not jobs and len(states) == NUM_SLOTS and all(s == "Unclaimed" for s in states):
            return
        time.sleep(1)
    raise AssertionError(
        f"pool did not go idle: {len(jobs)} jobs queued, slot states {states}"
    )


def _set_negotiator_running(pool, running, timeout=120):
    """Turn the negotiator off or on, and wait until its collector ad says so."""
    rv = pool.run_command(["condor_on" if running else "condor_off", "-daemon", "negotiator"])
    assert rv.returncode == 0, rv.stderr
    deadline = time.time() + timeout
    while time.time() < deadline:
        ads = pool.status(ad_type=htcondor.AdTypes.Negotiator, projection=["Name"])
        if bool(ads) == running:
            return
        time.sleep(0.5)
    raise AssertionError(f"negotiator did not turn {'on' if running else 'off'} within {timeout}s")


def _wait_for_idle_submitters(pool, idle_by_submitter, timeout=120):
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
    raise AssertionError(f"submitter ads never showed {idle_by_submitter} idle; saw {idle}")


def _set_priority_config(pool, constraint: Optional[str], slot_constraint: Optional[str]):
    """Append the priority-round knobs to the config file.  Later definitions
    win, so this replaces whatever an earlier scenario set.  Only a negotiator
    started after this sees the change."""
    with pool.config_file.open("a") as f:
        f.write("\n# priority round for the next scenario\n")
        f.write(f"NEGOTIATOR_PRIORITY_SUBMITTER_CONSTRAINT = {constraint or ''}\n")
        f.write(f"NEGOTIATOR_PRIORITY_SLOT_CONSTRAINT = {slot_constraint or ''}\n")


def _wait_for_slot_states(pool, want, timeout=120):
    """Wait until the collector shows each slot named in want (slot1, ...) in
    the given state."""
    deadline = time.time() + timeout
    states = {}
    while time.time() < deadline:
        slots = pool.status(ad_type=htcondor.AdTypes.Startd, projection=["Name", "State"])
        states = {s.get("Name", "").split("@")[0]: s.get("State") for s in slots}
        if all(states.get(name) == state for name, state in want.items()):
            return
        time.sleep(1)
    raise AssertionError(f"collector never showed slot states {want}; saw {states}")


def _negotiate(pool, releases, idle_by_submitter, constraint, slot_constraint):
    """With the negotiator off: set the knobs, release the jobs, and wait until
    the collector shows every submitter's jobs idle.  Then turn it on."""
    _set_negotiator_running(pool, False)
    _set_priority_config(pool, constraint, slot_constraint)
    with pool.use_config():
        for schedd, cluster in releases:
            schedd.act(htcondor.JobAction.Release, f"ClusterId == {cluster}")
    _wait_for_idle_submitters(pool, idle_by_submitter)
    _set_negotiator_running(pool, True)
    pool.run_command(["condor_reschedule"])


class Split(NamedTuple):
    favored: int
    other: int
    round_ran: bool
    round_matched: int  # most slots any one priority round matched
    slots: dict         # "f", "g", "o" -> sorted names of the slots they hold


def _run_scenario(
    pool, schedds, favored_schedd_ad, test_dir, path_to_sleep, name,
    prioritize, second_favored=False, slot_constraint=None,
):
    regular, favored = schedds
    group_f, group_g, group_o = f"{name}f", f"{name}g", f"{name}o"
    submitter_f = _submitter_name(pool, group_f)
    submitter_g = _submitter_name(pool, group_g)
    submitter_o = _submitter_name(pool, group_o)
    constraint = None
    if prioritize:
        constraint = f'ScheddName == "{favored_schedd_ad["Name"]}"'

    _wait_for_idle_pool(pool, schedds)

    # Skip what earlier scenarios logged: read() keeps every message it has
    # parsed in .messages until clear().
    neg_log = pool.negotiator_log.open()
    for _ in neg_log.read():
        pass
    neg_log.clear()

    try:
        held_f = _submit_held(pool, favored, test_dir, path_to_sleep, group_f, NUM_SLOTS)
        held_o = _submit_held(pool, regular, test_dir, path_to_sleep, group_o, NUM_SLOTS)
        releases = [(favored, held_f), (regular, held_o)]
        idle = {submitter_f: NUM_SLOTS, submitter_o: NUM_SLOTS}
        if second_favored:
            held_g = _submit_held(pool, favored, test_dir, path_to_sleep, group_g, NUM_SLOTS)
            releases.append((favored, held_g))
            idle[submitter_g] = NUM_SLOTS
        _negotiate(pool, releases, idle, constraint, slot_constraint)

        counts = _wait_for_running(pool, schedds, NUM_SLOTS)
        by_group = _slots_by_group(pool, schedds)
        slots = {role: by_group.get(group, []) for role, group in
                 (("f", group_f), ("g", group_g), ("o", group_o))}
        for _ in neg_log.read():
            pass
        round_ran = any(ROUND_RAN.search(m.message) for m in neg_log.messages)
        round_matched = max(
            (int(mo.group(1)) for m in neg_log.messages
             if (mo := ROUND_MATCHED.search(m.message))),
            default=0,
        )
        logger.info(f"scenario {name}: running by group {counts}, slots {slots}, "
                    f"priority round ran: {round_ran}, most matched in one round: {round_matched}")
        return Split(
            favored=counts.get(group_f, 0) + counts.get(group_g, 0),
            other=counts.get(group_o, 0),
            round_ran=round_ran,
            round_matched=round_matched,
            slots=slots,
        )
    finally:
        # A failure above may have left the negotiator off.
        _set_negotiator_running(pool, True)
        with pool.use_config():
            for schedd in schedds:
                schedd.act(htcondor.JobAction.Remove, "true")
        _wait_for_idle_pool(pool, schedds)


@action
def control_split(pool, schedds, favored_schedd_ad, test_dir, path_to_sleep):
    return _run_scenario(pool, schedds, favored_schedd_ad, test_dir, path_to_sleep, "ctl", prioritize=False)


@action
def whole_pool_split(pool, schedds, favored_schedd_ad, test_dir, path_to_sleep):
    return _run_scenario(pool, schedds, favored_schedd_ad, test_dir, path_to_sleep, "all", prioritize=True)


@action
def slot_subset_split(pool, schedds, favored_schedd_ad, test_dir, path_to_sleep):
    return _run_scenario(
        pool, schedds, favored_schedd_ad, test_dir, path_to_sleep, "sub",
        prioritize=True, slot_constraint="SlotID <= 3",
    )


@action
def subset_pie_split(pool, schedds, favored_schedd_ad, test_dir, path_to_sleep):
    return _run_scenario(
        pool, schedds, favored_schedd_ad, test_dir, path_to_sleep, "pie",
        prioritize=True, second_favored=True, slot_constraint="SlotID <= 2",
    )


def _run_reclaim_scenario(pool, schedds, favored_schedd_ad, test_dir, path_to_sleep):
    """F, favored on slots 1-3, fills the pool while O has no jobs.  Then O's
    jobs arrive and F's job on slot 2 is removed.  Returns the slots F and O
    hold once the pool is full again."""
    regular, favored = schedds
    group_f, group_o = "rclf", "rclo"
    submitter_f = _submitter_name(pool, group_f)
    submitter_o = _submitter_name(pool, group_o)
    constraint = f'ScheddName == "{favored_schedd_ad["Name"]}"'
    slot_constraint = "SlotID <= 3"

    _wait_for_idle_pool(pool, schedds)
    try:
        # More jobs than slots, so F still has idle jobs when slot 2 frees up.
        held_f = _submit_held(pool, favored, test_dir, path_to_sleep, group_f, NUM_SLOTS + 2)
        _negotiate(pool, [(favored, held_f)], {submitter_f: NUM_SLOTS + 2},
                   constraint, slot_constraint)
        _wait_for_running(pool, schedds, NUM_SLOTS)
        before = _slots_by_group(pool, schedds)
        assert before.get(group_f) == ["slot1", "slot2", "slot3", "slot4"], before

        held_o = _submit_held(pool, regular, test_dir, path_to_sleep, group_o, NUM_SLOTS)
        _set_negotiator_running(pool, False)
        with pool.use_config():
            # Hold F's idle jobs first, or the schedd hands slot 2's claim
            # straight to one of them when the job on it is removed.
            favored.act(htcondor.JobAction.Hold, f"ClusterId == {held_f} && JobStatus == 1")
            on_slot2 = [
                ad for ad in favored.query(constraint="JobStatus == 2",
                                           projection=["ClusterId", "ProcId", "RemoteHost"])
                if ad.get("RemoteHost", "").startswith("slot2@")
            ]
            assert len(on_slot2) == 1, on_slot2
            job = on_slot2[0]
            favored.act(htcondor.JobAction.Remove, [f"{job['ClusterId']}.{job['ProcId']}"])
        # The fresh negotiator works out usage from the slot ads in the
        # collector, so they have to show what F holds now.
        _wait_for_slot_states(pool, {"slot1": "Claimed", "slot2": "Unclaimed",
                                     "slot3": "Claimed", "slot4": "Claimed"})
        with pool.use_config():
            favored.act(htcondor.JobAction.Release, f"ClusterId == {held_f}")
            regular.act(htcondor.JobAction.Release, f"ClusterId == {held_o}")
        _wait_for_idle_submitters(pool, {submitter_f: 2, submitter_o: NUM_SLOTS})
        _set_negotiator_running(pool, True)
        pool.run_command(["condor_reschedule"])

        _wait_for_running(pool, schedds, NUM_SLOTS)
        after = _slots_by_group(pool, schedds)
        logger.info(f"scenario rcl: slots before {before}, after {after}")
        return {"f": after.get(group_f, []), "o": after.get(group_o, [])}
    finally:
        # A failure above may have left the negotiator off.
        _set_negotiator_running(pool, True)
        with pool.use_config():
            for schedd in schedds:
                schedd.act(htcondor.JobAction.Remove, "true")
        _wait_for_idle_pool(pool, schedds)


@action
def reclaim_slots(pool, schedds, favored_schedd_ad, test_dir, path_to_sleep):
    return _run_reclaim_scenario(pool, schedds, favored_schedd_ad, test_dir, path_to_sleep)


@pytest.mark.skipif(
    sys.platform == "win32",
    reason="needs jobs that stay running: on the Windows CI runner the "
    "sleep.cmd polyglot exits at once",
)
class TestPriorityRound:

    def test_control_splits_evenly(self, control_split):
        # If this fails, the other scenarios' expected splits mean nothing.
        assert (control_split.favored, control_split.other) == (NUM_SLOTS // 2, NUM_SLOTS // 2)
        assert not control_split.round_ran

    def test_round_takes_whole_pool(self, whole_pool_split):
        # With no slot constraint, the round may match any free slot.
        assert whole_pool_split.round_ran
        assert whole_pool_split.round_matched == NUM_SLOTS
        assert (whole_pool_split.favored, whole_pool_split.other) == (NUM_SLOTS, 0)

    def test_slot_constraint_limits_round(self, slot_subset_split):
        # The round takes only slots 1-3; F, over its fair share after that,
        # gets nothing more, and the fair-share round gives O slot 4.
        assert slot_subset_split.round_ran
        assert slot_subset_split.round_matched == 3
        assert slot_subset_split.slots["f"] == ["slot1", "slot2", "slot3"]
        assert slot_subset_split.slots["o"] == ["slot4"]

    def test_slot_constraint_sizes_pie(self, subset_pie_split):
        # The round's pie is the 2 slots it may use, so F and G get one each.
        assert subset_pie_split.round_ran
        assert subset_pie_split.round_matched == 2
        # Slots 1 and 2 went in the round, so only F or G can hold them.
        round_slots = {"slot1", "slot2"}
        assert len(round_slots & set(subset_pie_split.slots["f"])) == 1
        assert len(round_slots & set(subset_pie_split.slots["g"])) == 1

    def test_round_ignores_usage_elsewhere(self, reclaim_slots):
        # F still holds slot 4, outside the round's slots, but that must not
        # cost it slot 2.
        assert reclaim_slots["f"] == ["slot1", "slot2", "slot3", "slot4"]
        assert reclaim_slots["o"] == []

    def test_cycle_stats_published(self, pool, slot_subset_split):
        ads = pool.status(
            ad_type=htcondor.AdTypes.Negotiator,
            projection=["LastNegotiationCyclePrioritySubmitters0",
                        "LastNegotiationCyclePriorityDuration0"],
        )
        assert ads
        # Published every cycle; the last scenario prioritized one submitter,
        # but later cycles may see no idle favored submitter, so only check
        # that the attributes are there.
        assert "LastNegotiationCyclePrioritySubmitters0" in ads[0]
        assert "LastNegotiationCyclePriorityDuration0" in ads[0]
