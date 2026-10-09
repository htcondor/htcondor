#!/usr/bin/env pytest

#
# Regression test: a large backlog of high-JobPrio jobs matching only
# SlotClass "A" could exhaust the schedd's per-negotiation offer budget
# (m_jobs_can_offer in src/condor_schedd.V6/schedd_negotiate.cpp) in the
# first resource request list, so a low-JobPrio SlotClass-"B" auto cluster
# was never offered to the negotiator, even though "B" slots sit idle.
#
# Once an auto cluster uses up the offer budget, the schedd now resets the
# budget to (original limit - matches received) and keeps going through the
# remaining auto clusters, so the "B" request is sent in the same list. This
# matters most with negotiator prefetch, where a list ending in NO_MORE_JOBS
# is never followed by another request for more.
#
# The "A" jobs are started first so every negotiation cycle is in the
# starving steady state: MAX_JOBS_RUNNING leaves a budget of 2, the "A"
# backlog of 8 consumes it all, and the "A" partitionable slot is fully
# carved up, leaving no "A" resources free.
#

import logging

from ornithology import (
    config,
    standup,
    action,

    Condor,
    ClusterState,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@config(params={"prefetch": "TRUE", "no_prefetch": "FALSE"})
def prefetch_requests(request):
    return request.param


@standup
def condor(test_dir, prefetch_requests):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "NUM_CPUS": "4",

            # One 2-core partitionable slot per class, each fits two
            # 1-core jobs.
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
            # cover the whole pool (2 A + 2 B cores).
            "MAX_JOBS_RUNNING": "4",

            "NEGOTIATOR_PREFETCH_REQUESTS": prefetch_requests,

            "NEGOTIATOR_DEBUG": "D_MATCH D_CATEGORY D_SUB_SECOND",
            "SCHEDD_DEBUG": "D_FULLDEBUG",
        },
    ) as condor:
        yield condor


@action
def slot_a_handle(condor, path_to_sleep):
    # High JobPrio backlog matching only SlotClass "A". These must outlive
    # the SlotClass-B wait below so the A slots stay busy.
    handle = condor.submit(
        description={
            "executable": path_to_sleep,
            "arguments": "600",
            "request_cpus": "1",
            "requirements": 'Target.SlotClass == "A"',
            "priority": "100",
            "request_memory": "1MB",
            "request_disk": "1MB",
            "log": "slot_a.log",
        },
        count=10,
    )

    assert handle.wait(
        condition=ClusterState.running_exactly(2),
        fail_condition=ClusterState.any_held,
        timeout=120,
        verbose=True,
    )

    yield handle

    handle.remove()


@action
def slot_b_handle(condor, slot_a_handle, path_to_sleep):
    # Low JobPrio, matches only the otherwise-idle SlotClass "B".
    return condor.submit(
        description={
            "executable": path_to_sleep,
            "arguments": "0",
            "request_cpus": "1",
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
        # Without the budget reset, these are never offered to the
        # negotiator while the SlotClass-A jobs run, and this times out.
        assert slot_b_handle.wait(
            condition=ClusterState.all_complete,
            fail_condition=ClusterState.any_held,
            timeout=180,
            verbose=True,
        )
