#!/usr/bin/env pytest

# Companion regression test to test_negotiator_resources_in_use_zeroed.py, for the
# *hierarchical group* half of the same accountant bug.
#
# Each negotiation cycle Accountant::CheckMatches() recomputes usage "from
# scratch" by zeroing the per-customer counters and re-adding one match per
# claimed slot with AddMatch().  But CheckMatches() only zeroes the flat
# WeightedResourcesUsed counter -- it never zeroes HierWeightedResourcesUsed,
# the counter AddMatch() accumulates up the accounting-group tree
# (Accountant.cpp AddMatch() walks GroupNamePart up the hierarchy bumping
# HierWeightedResourcesUsed at each node).  So for any accounting group, the
# hierarchical usage climbs by the group's claimed-slot count on every cycle and
# never comes back down.
#
# This matters beyond the raw report: UpdateOnePriority() *overrides* a group
# node's recent usage with HierWeightedResourcesUsed whenever it is > 0, so a
# group's effective priority is driven by this runaway value -- group fair-share
# is progressively corrupted the longer the negotiator runs.
#
# As in the sibling test, we need no schedd/startd/jobs: we stand up a
# central-manager-only pool, define an accounting group, and advertise fake
# claimed slot ads whose AccountingGroup places them under that group.  The
# accountant charges the group on every cycle purely from the machine ads.
#
# Oracle: the group's WeightedResourcesUsed *is* zeroed each cycle, so it
# correctly holds at the number of claimed slots; HierWeightedResourcesUsed
# should equal it.  With the fix both equal NUM_SLOTS every cycle; with the bug
# HierWeightedResourcesUsed climbs (NUM_SLOTS, 2*NUM_SLOTS, ...) while
# WeightedResourcesUsed stays put.

import logging
import time

import classad2 as classad
import htcondor2 as htcondor

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

NUM_SLOTS = 3

# The accounting group the fake slots are charged to, and a submitter within it.
GROUP = "group_a"
FAKE_USER = "{}.fakeuser@fake.example.com".format(GROUP)

_FAKE_ADDR = "<127.0.0.1:19998?addrs=127.0.0.1-19998&noUDP>"


@standup
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "DAEMON_LIST": "MASTER COLLECTOR NEGOTIATOR",
            "USE_SHARED_PORT": False,
            "NEGOTIATOR_INTERVAL": "5",
            "NEGOTIATOR_CYCLE_DELAY": "1",
            "NEGOTIATOR_MIN_INTERVAL": "1",
            "NEGOTIATOR_DEBUG": "D_ACCOUNTANT",
            # Define a single flat accounting group so the accountant builds a
            # tree node the fake slots can be charged against.
            "GROUP_NAMES": GROUP,
            "GROUP_QUOTA_{}".format(GROUP): "10",
        },
    ) as condor:
        yield condor


def _slot_ad(index):
    """A fake startd slot ad, Claimed under the GROUP accounting group."""
    name = "slot{}@fakehost{:02d}.example.com".format(index, index)
    return classad.ClassAd(
        {
            "MyType": "Machine",
            "TargetType": "Job",
            "Name": name,
            "Machine": "fakehost{:02d}.example.com".format(index),
            "MyAddress": _FAKE_ADDR,
            "StartdIpAddr": "127.0.0.1",
            "State": "Claimed",
            "Activity": "Busy",
            # AccountingGroup (preferred over RemoteUser by Accountant::IsClaimed)
            # is what charges the slot to GROUP.
            "AccountingGroup": FAKE_USER,
            "RemoteUser": FAKE_USER,
            "Cpus": 1,
            "Memory": 1024,
            "Disk": 1048576,
            "SlotWeight": 1.0,
            # Long lifetime so a slot cannot silently age out of the collector if
            # an iteration stalls on a badly oversubscribed CI machine (we also
            # re-advertise every cycle).
            "ClassAdLifetime": 3600,
            "IsPytest": True,
        }
    )


def _advertise_slots(collector):
    ads = [_slot_ad(i) for i in range(1, NUM_SLOTS + 1)]
    collector.advertise(ads, "UPDATE_STARTD_AD")
    deadline = time.time() + 90
    while time.time() < deadline:
        found = collector.query(htcondor.AdTypes.Startd, "IsPytest == true")
        if len(found) >= NUM_SLOTS:
            return
        time.sleep(0.5)
    raise AssertionError(
        "fake slot ads never appeared in the collector (wanted {})".format(NUM_SLOTS)
    )


def _group_usage(condor, group):
    """Return (WeightedResourcesUsed, HierWeightedResourcesUsed) the accountant
    records for accounting-group node `group`, or None if the group record does
    not exist yet."""
    with condor.use_config():
        neg = htcondor.Negotiator()
        ads = neg.getPriorities()
    for ad in ads:
        # Group nodes report IsAccountingGroup == True and Name == the group name.
        if ad.get("Name") == group and ad.get("IsAccountingGroup", False):
            return (
                float(ad["WeightedResourcesUsed"]),
                float(ad["HierWeightedResourcesUsed"]),
            )
    return None


class TestNegotiatorGroupHierUsageZeroed:

    def test_hier_weighted_usage_is_bounded(self, condor):
        collector = condor.get_local_collector()

        neg_log = condor.negotiator_log.open()

        def drain():
            for _ in neg_log.read():
                pass

        def wait_for_finished_cycle(timeout=150):
            assert neg_log.wait(
                condition=lambda m: "Finished Negotiation Cycle" in m.message,
                timeout=timeout,
            ), "negotiator did not finish a cycle within {}s".format(timeout)

        _advertise_slots(collector)

        # Keep only samples from a cycle that charged the group for all NUM_SLOTS
        # slots (WeightedResourcesUsed, the correctly-zeroed counter, reads exactly
        # NUM_SLOTS).  A transient under-count -- an ad still propagating, a cycle
        # racing our re-advertise -- is skipped, not failed, so an oversubscribed
        # CI machine can't produce a spurious failure.  Bug detection is unaffected:
        # under the bug a full-count cycle still shows HierWeightedResourcesUsed
        # climbing while WeightedResourcesUsed holds at NUM_SLOTS.
        samples = []
        MAX_CYCLES = 20
        NEEDED_SAMPLES = 4

        for _ in range(MAX_CYCLES):
            _advertise_slots(collector)
            drain()
            condor.run_command(["condor_reschedule"])
            wait_for_finished_cycle()

            usage = _group_usage(condor, GROUP)
            if usage is None or usage[0] != NUM_SLOTS:
                # Group record not present yet, or a cycle that didn't charge every
                # slot; don't measure a partial cycle.
                continue
            samples.append(usage)
            logger.debug(
                "cycle sample: group %s WeightedResourcesUsed=%g "
                "HierWeightedResourcesUsed=%g",
                GROUP,
                usage[0],
                usage[1],
            )
            if len(samples) >= NEEDED_SAMPLES:
                break

        assert len(samples) >= NEEDED_SAMPLES, (
            "accountant never reported {} full-count cycles of usage for group {} "
            "(got {} samples: {})".format(NEEDED_SAMPLES, GROUP, len(samples), samples)
        )

        weighted_used = [wru for (wru, _hier) in samples]
        hier_used = [hier for (_wru, hier) in samples]

        # Regression check: hierarchical usage must track the weighted counter --
        # i.e. stay pinned at NUM_SLOTS.  Under the bug it climbs by NUM_SLOTS
        # each cycle because CheckMatches() never zeroes HierWeightedResourcesUsed,
        # which then corrupts the group's effective priority.
        assert all(hier == NUM_SLOTS for hier in hier_used), (
            "HierWeightedResourcesUsed is not bounded: expected {} every cycle but "
            "observed {} -- the accountant's hierarchical group usage is never "
            "zeroed and grows without bound (WeightedResourcesUsed, which IS "
            "zeroed each cycle, stayed at {})".format(
                NUM_SLOTS, hier_used, weighted_used
            )
        )
