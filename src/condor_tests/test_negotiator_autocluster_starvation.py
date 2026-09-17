#!/usr/bin/env pytest

#
# Regression test: a large backlog of high-JobPrio jobs matching only
# SlotClass "A" could exhaust the schedd's per-session offer budget
# (ScheddNegotiate::nextJob(), src/condor_schedd.V6/schedd_negotiate.cpp)
# before ever offering a low-JobPrio SlotClass-"B" auto cluster, starving
# it even though "B" slots sit idle.
#
# Uses a small MAX_JOBS_RUNNING and an oversized SlotClass-A backlog to
# trigger the clamp deterministically, then asserts the SlotClass-B jobs
# still complete.
#

import logging

from ornithology import (
    standup,
    action,

    Condor,
    ClusterState,
    JobStatus,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@standup
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "NUM_CPUS": "4",

            # One 2-cpu partitionable slot per SlotClass, so each class
            # can still host 2 concurrent 1-cpu jobs, like real pools.
            "SLOT_TYPE_1": "cpus=2",
            "SLOT_TYPE_1_PARTITIONABLE": "TRUE",
            "NUM_SLOTS_TYPE_1": "1",
            "SLOT_TYPE_1_SlotClass": '"A"',

            "SLOT_TYPE_2": "cpus=2",
            "SLOT_TYPE_2_PARTITIONABLE": "TRUE",
            "NUM_SLOTS_TYPE_2": "1",
            "SLOT_TYPE_2_SlotClass": '"B"',

            "STARTD_ATTRS": "$(STARTD_ATTRS) SlotClass",

            # Smaller than the SlotClass-A backlog, but large enough to
            # cover the whole pool (2 A + 2 B cpus).
            "MAX_JOBS_RUNNING": "4",

            # Fast negotiation cycles for a quick test.
            "NEGOTIATOR_INTERVAL": "1",
            "NEGOTIATOR_MIN_INTERVAL": "1",
            "NEGOTIATOR_CYCLE_DELAY": "1",
            "NEGOTIATOR_DEBUG": "D_MATCH D_CATEGORY D_SUB_SECOND",
            "SCHEDD_DEBUG": "D_FULLDEBUG",
            "SCHEDD_MIN_INTERVAL": "0",
            "SCHEDD_INTERVAL_TIMESLICE": "1",
        },
    ) as condor:
        yield condor


@action
def slot_a_handle(condor, path_to_sleep):
    # High JobPrio backlog matching only SlotClass "A".
    return condor.submit(
        description={
            "executable": path_to_sleep,
            "arguments": "300",
            "requirements": 'Target.SlotClass == "A"',
            "priority": "100",
            "request_memory": "1MB",
            "request_disk": "1MB",
            "log": "slot_a.log",
        },
        count=10,
    )


@action
def slot_b_handle(condor, slot_a_handle, path_to_sleep):
    # Low JobPrio, matches only the otherwise-idle SlotClass "B".
    return condor.submit(
        description={
            "executable": path_to_sleep,
            "arguments": "0",
            "requirements": 'Target.SlotClass == "B"',
            "priority": "0",
            "request_memory": "1MB",
            "request_disk": "1MB",
            "log": "slot_b.log",
        },
        count=2,
    )


class TestNegotiatorAutoclusterStarvation:
    def test_low_priority_slot_class_not_starved(self, slot_b_handle):
        # Pre-fix, these were never offered to the negotiator and this timed out.
        assert slot_b_handle.wait(
            condition=ClusterState.all_complete,
            fail_condition=ClusterState.any_held,
            timeout=180,
            verbose=True,
        )


#
# Regression test for the zero-match variant of the same bug: the
# high-JobPrio auto cluster here can never match *any* slot in the pool
# (its requirements target a SlotClass that doesn't exist), so the
# negotiator rejects every job in it outright. Pre-fix, that first
# rejection still permanently consumed the whole session offer budget
# (ScheddNegotiate::nextJob() clamps and deducts speculatively before
# any match is confirmed), so the low-JobPrio auto cluster was never
# even offered to the negotiator, even though its slots sit idle. This
# mirrors the real-world report of MAX_CONCURRENT_UPLOADS_PER_USER (a
# per-user session-budget curb from HTCONDOR-3664) starving a user's
# other auto clusters whenever their highest-priority one fails to
# match anything.
#


@action
def unmatchable_handle(condor, path_to_sleep):
    # High JobPrio backlog whose requirements match no slot in the pool
    # at all (there is no SlotClass "C") -- every job in this auto
    # cluster gets rejected outright, with zero matches ever confirmed.
    return condor.submit(
        description={
            "executable": path_to_sleep,
            "arguments": "300",
            "requirements": 'Target.SlotClass == "C"',
            "priority": "100",
            "request_memory": "1MB",
            "request_disk": "1MB",
            "log": "unmatchable.log",
        },
        count=10,
    )


@action
def matchable_handle(condor, unmatchable_handle, path_to_sleep):
    # Low JobPrio, matches the otherwise-idle SlotClass "B" slots.
    return condor.submit(
        description={
            "executable": path_to_sleep,
            "arguments": "0",
            "requirements": 'Target.SlotClass == "B"',
            "priority": "0",
            "request_memory": "1MB",
            "request_disk": "1MB",
            "log": "matchable.log",
        },
        count=2,
    )


class TestNegotiatorZeroMatchAutoclusterStarvation:
    def test_low_priority_auto_cluster_not_starved_by_total_rejection(
        self, condor, matchable_handle
    ):
        # Pre-fix, the unmatchable auto cluster's rejection never refunded
        # the session budget, so these were never offered and this timed out.
        assert matchable_handle.wait(
            condition=ClusterState.all_complete,
            fail_condition=ClusterState.any_held,
            timeout=180,
            verbose=True,
        )

        # By default the negotiator advertises "ScheddOfferCap" and the
        # schedd reports true demand (SCHEDD_USE_TRUE_DEMAND_REPORTING),
        # so the legacy clamp-and-refund path should never engage here --
        # confirm that, rather than just relying on it having also worked.
        schedd_log_text = condor.schedd_log.path.read_text()
        assert "Refunding" not in schedd_log_text


#
# Backward-compatibility regression test: with the negotiator's
# "ScheddOfferCap" capability advertisement disabled (simulating an older
# negotiator that doesn't understand ATTR_SCHEDD_OFFER_LIMIT), the schedd
# must fall back to its pre-existing clamp-and-refund path
# (SCHEDD_REFUND_UNUSED_RESOURCE_REQUESTS) and still avoid starving the
# low-priority auto cluster -- proving new-schedd/old-negotiator pools
# behave exactly as they did before this redesign.
#


@standup
def condor_legacy_negotiator(test_dir):
    with Condor(
        local_dir=test_dir / "condor_legacy_negotiator",
        config={
            "NUM_CPUS": "4",

            "SLOT_TYPE_1": "cpus=2",
            "SLOT_TYPE_1_PARTITIONABLE": "TRUE",
            "NUM_SLOTS_TYPE_1": "1",
            "SLOT_TYPE_1_SlotClass": '"A"',

            "SLOT_TYPE_2": "cpus=2",
            "SLOT_TYPE_2_PARTITIONABLE": "TRUE",
            "NUM_SLOTS_TYPE_2": "1",
            "SLOT_TYPE_2_SlotClass": '"B"',

            "STARTD_ATTRS": "$(STARTD_ATTRS) SlotClass",

            "MAX_JOBS_RUNNING": "4",

            # Force the negotiator to behave like an older, pre-redesign
            # negotiator: it never advertises (or honors) the
            # ScheddOfferCap match capability.
            "NEGOTIATOR_ADVERTISE_SCHEDD_OFFER_CAP": "FALSE",

            "NEGOTIATOR_INTERVAL": "1",
            "NEGOTIATOR_MIN_INTERVAL": "1",
            "NEGOTIATOR_CYCLE_DELAY": "1",
            "NEGOTIATOR_DEBUG": "D_MATCH D_CATEGORY D_SUB_SECOND",
            "SCHEDD_DEBUG": "D_FULLDEBUG",
            "SCHEDD_MIN_INTERVAL": "0",
            "SCHEDD_INTERVAL_TIMESLICE": "1",
        },
    ) as condor:
        yield condor


@action
def unmatchable_handle_legacy(condor_legacy_negotiator, path_to_sleep):
    return condor_legacy_negotiator.submit(
        description={
            "executable": path_to_sleep,
            "arguments": "300",
            "requirements": 'Target.SlotClass == "C"',
            "priority": "100",
            "request_memory": "1MB",
            "request_disk": "1MB",
            "log": "unmatchable_legacy.log",
        },
        count=10,
    )


@action
def matchable_handle_legacy(condor_legacy_negotiator, unmatchable_handle_legacy, path_to_sleep):
    return condor_legacy_negotiator.submit(
        description={
            "executable": path_to_sleep,
            "arguments": "0",
            "requirements": 'Target.SlotClass == "B"',
            "priority": "0",
            "request_memory": "1MB",
            "request_disk": "1MB",
            "log": "matchable_legacy.log",
        },
        count=2,
    )


class TestNegotiatorLegacyFallback:
    def test_legacy_refund_path_still_prevents_starvation(
        self, condor_legacy_negotiator, matchable_handle_legacy
    ):
        assert matchable_handle_legacy.wait(
            condition=ClusterState.all_complete,
            fail_condition=ClusterState.any_held,
            timeout=180,
            verbose=True,
        )

        # Confirm the *legacy* clamp-and-refund path actually engaged --
        # not that the new true-demand path silently did the work instead
        # (which would defeat the point of this compatibility test).
        schedd_log_text = condor_legacy_negotiator.schedd_log.path.read_text()
        assert "Refunding" in schedd_log_text


#
# Safety regression test: with true-demand reporting enabled, the schedd
# reports a large, uncapped ATTR_RESOURCE_REQUEST_COUNT (the whole
# backlog) and relies entirely on the negotiator to enforce
# ATTR_SCHEDD_OFFER_LIMIT. This confirms the negotiator's new cap check
# (Matchmaker::negotiate(), matchmaker.cpp) actually holds the line at
# MAX_JOBS_RUNNING even though real slot capacity in the pool exceeds it
# -- i.e. that reporting true demand didn't just move the starvation bug
# into an "overrun" bug.
#


@standup
def condor_oversubscribed(test_dir):
    with Condor(
        local_dir=test_dir / "condor_oversubscribed",
        config={
            # More real capacity than MAX_JOBS_RUNNING allows, so the cap
            # can only be respected if the negotiator actually enforces
            # ATTR_SCHEDD_OFFER_LIMIT itself.
            "NUM_CPUS": "8",
            "MAX_JOBS_RUNNING": "4",

            "NEGOTIATOR_INTERVAL": "1",
            "NEGOTIATOR_MIN_INTERVAL": "1",
            "NEGOTIATOR_CYCLE_DELAY": "1",
            "NEGOTIATOR_DEBUG": "D_MATCH D_CATEGORY D_SUB_SECOND",
            "SCHEDD_DEBUG": "D_FULLDEBUG",
            "SCHEDD_MIN_INTERVAL": "0",
            "SCHEDD_INTERVAL_TIMESLICE": "1",
        },
    ) as condor:
        yield condor


@action
def oversubscribed_handle(condor_oversubscribed, path_to_sleep):
    # All same priority/auto cluster; long enough to observe steady state.
    return condor_oversubscribed.submit(
        description={
            "executable": path_to_sleep,
            "arguments": "300",
            "request_memory": "1MB",
            "request_disk": "1MB",
            "log": "oversubscribed.log",
        },
        count=8,
    )


class TestNegotiatorEnforcesScheddOfferCap:
    def test_running_count_never_exceeds_max_jobs_running(
        self, condor_oversubscribed, oversubscribed_handle
    ):
        max_running = 4
        assert oversubscribed_handle.wait(
            condition=ClusterState.running_exactly(max_running),
            fail_condition=lambda cs: cs.counts()[JobStatus.RUNNING] > max_running,
            timeout=180,
            verbose=True,
        )
