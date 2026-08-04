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
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@standup
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "NUM_CPUS": "4",

            "SLOT_TYPE_1": "cpus=1",
            "SLOT_TYPE_1_PARTITIONABLE": "FALSE",
            "NUM_SLOTS_TYPE_1": "2",
            "SLOT_TYPE_1_SlotClass": '"A"',

            "SLOT_TYPE_2": "cpus=1",
            "SLOT_TYPE_2_PARTITIONABLE": "FALSE",
            "NUM_SLOTS_TYPE_2": "2",
            "SLOT_TYPE_2_SlotClass": '"B"',

            "STARTD_ATTRS": "$(STARTD_ATTRS) SlotClass",

            # Smaller than the SlotClass-A backlog, but large enough to
            # cover the whole pool (2 A + 2 B slots).
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
