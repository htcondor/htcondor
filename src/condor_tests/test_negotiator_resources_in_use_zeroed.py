#!/usr/bin/env pytest

# Regression test for the accountant's per-submitter ResourcesUsed counter
# growing without bound.
#
# Each negotiation cycle the accountant recomputes usage "from scratch":
# Accountant::CheckMatches() zeroes the per-customer usage counters and then
# re-adds one match per currently-claimed slot via AddMatch().  The bug is that
# CheckMatches() only zeroes the *weighted* counter (WeightedResourcesUsed),
# while AddMatch() bumps *both* the weighted counter and the un-weighted integer
# counter (ResourcesUsed).  With nothing ever zeroing ResourcesUsed, it climbs by
# the number of claimed slots on every single cycle and never comes back down --
# so a long-running negotiator reports ever-larger "resources in use" for a user
# whose actual footprint is constant.
#
# We exercise this cheaply -- no schedd, no startd, no real jobs.  We stand up a
# central-manager-only pool (collector + negotiator/accountant) and advertise a
# fixed set of *fake* claimed slot ads directly to the collector with the python
# bindings.  The accountant attributes those claimed slots to their RemoteUser on
# every cycle (this path is driven entirely by the machine ads, independent of any
# submitter), so we can watch the counters evolve across several cycles.
#
# The oracle is the accountant itself: WeightedResourcesUsed is the counter that
# *is* zeroed each cycle, so it correctly stays pinned at the number of claimed
# slots.  The (buggy) un-weighted ResourcesUsed should track it exactly.  With the
# fix, ResourcesUsed == WeightedResourcesUsed == NUM_SLOTS on every cycle; with the
# bug, ResourcesUsed diverges upward while WeightedResourcesUsed holds steady.

import logging
import time

import classad2 as classad
import htcondor2 as htcondor

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# Number of fake claimed slots we pin against a single fake user.  Each has
# SlotWeight 1, so both the weighted and un-weighted usage should read exactly
# this value every cycle.
NUM_SLOTS = 3

# The (entirely fictional) user we claim every slot for.
FAKE_USER = "fakeuser@fake.example.com"

# A plausible-looking but bogus sinful string; the negotiator never connects to
# these slots, it only reads their ads out of the collector.
_FAKE_ADDR = "<127.0.0.1:19999?addrs=127.0.0.1-19999&noUDP>"


@standup
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            # Central manager only: no schedd, no startd, no real jobs.  The
            # accountant lives inside the negotiator.
            "DAEMON_LIST": "MASTER COLLECTOR NEGOTIATOR",
            "USE_SHARED_PORT": False,
            # Drive cycles quickly and deterministically.
            "NEGOTIATOR_INTERVAL": "5",
            "NEGOTIATOR_CYCLE_DELAY": "1",
            "NEGOTIATOR_MIN_INTERVAL": "1",
            "NEGOTIATOR_DEBUG": "D_ACCOUNTANT",
        },
    ) as condor:
        yield condor


def _slot_ad(index):
    """A fake startd slot ad, Claimed by FAKE_USER with SlotWeight 1."""
    name = "slot{}@fakehost{:02d}.example.com".format(index, index)
    return classad.ClassAd(
        {
            "MyType": "Machine",
            "TargetType": "Job",
            "Name": name,
            "Machine": "fakehost{:02d}.example.com".format(index),
            "MyAddress": _FAKE_ADDR,
            "StartdIpAddr": "127.0.0.1",
            # These three attributes are what make the accountant count the slot
            # against FAKE_USER (Accountant::IsClaimed()).
            "State": "Claimed",
            "Activity": "Busy",
            "RemoteUser": FAKE_USER,
            # SlotWeight 1 so weighted usage == un-weighted usage == NUM_SLOTS.
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
    """(Re-)advertise the NUM_SLOTS fake claimed slots and wait for the collector
    to reflect all of them.  Re-advertising each round keeps the ads from aging
    out of the collector between cycles."""
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


def _usage_for(condor, user):
    """Return the accountant's usage counters for `user` as a dict, or None if
    the accountant has no record for the user yet.

    ResourcesUsed / AccumulatedUsage are the un-weighted counters implicated in
    the bug; WeightedResourcesUsed / WeightedAccumulatedUsage are their correctly
    maintained (zeroed-each-cycle) counterparts, used here as the oracle."""
    with condor.use_config():
        neg = htcondor.Negotiator()
        ads = neg.getPriorities()
    for ad in ads:
        if ad.get("Name") == user:
            return {
                "ResourcesUsed": int(ad["ResourcesUsed"]),
                "WeightedResourcesUsed": float(ad["WeightedResourcesUsed"]),
                "AccumulatedUsage": float(ad["AccumulatedUsage"]),
                "WeightedAccumulatedUsage": float(ad["WeightedAccumulatedUsage"]),
            }
    return None


class TestNegotiatorResourcesInUseZeroed:

    def test_resources_used_is_bounded(self, condor):
        collector = condor.get_local_collector()

        # Open the negotiator log so we can block on cycle boundaries.
        neg_log = condor.negotiator_log.open()

        def drain():
            # Consume everything written so far so the next wait() blocks on a
            # genuinely new line rather than returning on stale history.
            for _ in neg_log.read():
                pass

        # Per-cycle wait is generous: on a heavily oversubscribed CI machine the
        # negotiator can be starved for many seconds between cycles.  The pass/fail
        # decision does not depend on this value -- only on how long we are willing
        # to wait -- so err large.  The overall ctest timeout still bounds us.
        def wait_for_finished_cycle(timeout=150):
            assert neg_log.wait(
                condition=lambda m: "Finished Negotiation Cycle" in m.message,
                timeout=timeout,
            ), "negotiator did not finish a cycle within {}s".format(timeout)

        # Prime the collector with the claimed slots before the measurement loop.
        _advertise_slots(collector)

        # Collect one sample per completed cycle.  We only KEEP a sample from a
        # cycle that saw all NUM_SLOTS slots -- i.e. WeightedResourcesUsed (the
        # correctly-zeroed counter) reads exactly NUM_SLOTS.  This makes the test
        # immune to a transient under-count (an ad still propagating through the
        # collector, a cycle racing our re-advertise): such a cycle is skipped, not
        # failed.  It does NOT weaken bug detection -- under the bug a full-count
        # cycle still shows ResourcesUsed climbing while WeightedResourcesUsed holds
        # at NUM_SLOTS.  Needing several such samples also implicitly proves the
        # accountant is charging the slots at all.
        samples = []
        MAX_CYCLES = 20
        NEEDED_SAMPLES = 4

        for _ in range(MAX_CYCLES):
            _advertise_slots(collector)  # keep the ads alive across cycles
            drain()
            condor.run_command(["condor_reschedule"])
            wait_for_finished_cycle()

            usage = _usage_for(condor, FAKE_USER)
            if usage is None or usage["WeightedResourcesUsed"] != NUM_SLOTS:
                # Record not present yet, or a cycle that didn't see every slot;
                # don't measure a partial cycle.
                continue
            samples.append(usage)
            logger.debug(
                "cycle sample: ResourcesUsed=%d WeightedResourcesUsed=%g "
                "AccumulatedUsage=%g WeightedAccumulatedUsage=%g",
                usage["ResourcesUsed"],
                usage["WeightedResourcesUsed"],
                usage["AccumulatedUsage"],
                usage["WeightedAccumulatedUsage"],
            )
            if len(samples) >= NEEDED_SAMPLES:
                break

        assert len(samples) >= NEEDED_SAMPLES, (
            "accountant never reported {} full-count cycles of usage for {} "
            "(got {} samples: {})".format(
                NEEDED_SAMPLES, FAKE_USER, len(samples), samples
            )
        )

        resources_used = [s["ResourcesUsed"] for s in samples]
        weighted_used = [s["WeightedResourcesUsed"] for s in samples]

        # The primary regression check: the un-weighted counter must track the
        # weighted one -- i.e. stay pinned at NUM_SLOTS.  Under the bug it grows
        # by NUM_SLOTS each cycle (NUM_SLOTS, 2*NUM_SLOTS, ...) because
        # CheckMatches() never zeroes it.
        assert all(ru == NUM_SLOTS for ru in resources_used), (
            "ResourcesUsed is not bounded: expected {} every cycle but observed "
            "{} -- the accountant's un-weighted usage counter is never zeroed and "
            "grows without bound (WeightedResourcesUsed, which IS zeroed each "
            "cycle, stayed at {})".format(NUM_SLOTS, resources_used, weighted_used)
        )

        # Downstream companion check: AccumulatedUsage (lifetime usage, which
        # fair-share history is built from) integrates ResourcesUsed*time each
        # cycle (UpdateOnePriority), while WeightedAccumulatedUsage integrates the
        # correctly-zeroed WeightedResourcesUsed*time.  With SlotWeight 1 the two
        # are computed by identical arithmetic and must stay equal; the inflated
        # ResourcesUsed makes AccumulatedUsage pull ahead (super-linearly), so the
        # bug silently corrupts historical usage too, not just the instantaneous
        # report.
        accumulated = [s["AccumulatedUsage"] for s in samples]
        weighted_accumulated = [s["WeightedAccumulatedUsage"] for s in samples]
        assert all(
            au <= wau * 1.05 + 1e-6
            for au, wau in zip(accumulated, weighted_accumulated)
        ), (
            "AccumulatedUsage diverges from WeightedAccumulatedUsage: observed "
            "AccumulatedUsage={} vs WeightedAccumulatedUsage={} -- the inflated "
            "un-weighted ResourcesUsed leaks into lifetime AccumulatedUsage via "
            "UpdateOnePriority()".format(accumulated, weighted_accumulated)
        )
