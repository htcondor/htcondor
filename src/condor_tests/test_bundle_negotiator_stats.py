#!/usr/bin/env pytest

# Test the slot bundle statistics the negotiator publishes in its daemon ad.
#
# Two kinds of attribute are published:
#
#   Current state (BundlesInProgress, BundleSlotsWanted, BundleSlotsMatchedTotal,
#   BundleMaxWaitSeconds) -- what the negotiator is filling right now.
#
#   Per-cycle history (LastNegotiationCycle*Bundle*<i>) -- the same ring buffer
#   convention as every other negotiation cycle statistic.
#
# The pool here is deliberately too small for the bundle: BUNDLE_N slots are
# requested but only NUM_CPUS < BUNDLE_N exist.  That is the interesting state
# for these statistics and the only one that holds still long enough to assert
# on -- a bundle that fills is aged out of the negotiator's tracking a few
# cycles later, whereas an unfillable one stays in progress, wanting slots, with
# its wait time climbing.  It is also the case the statistics exist to expose: a
# bundle asking for more than the pool can ever offer at once waits forever by
# construction, and BundleMaxWaitSeconds is how an admin sees that.

import logging
import getpass
import time

import htcondor2 as htcondor
import classad2 as classad

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

NUM_CPUS = 2
BUNDLE_N = 4  # deliberately more than the pool has

CURRENT_STATE_ATTRS = [
	"BundlesInProgress",
	"BundleSlotsWanted",
	"BundleSlotsMatchedTotal",
	"BundleMaxWaitSeconds",
]


@standup
def condor(test_dir):
	with Condor(
		test_dir / "condor",
		config={
			"NUM_CPUS": str(NUM_CPUS),
			"MEMORY": "4096",
			"DISK": "4096000",
			"NEGOTIATOR_CYCLE_DELAY": "2",
			"NEGOTIATOR_INTERVAL": "5",
			"SCHEDD_INTERVAL": "5",
			# The negotiator ad is only re-sent on this interval, so keep it
			# short or the test spends its life waiting for a refresh.
			"NEGOTIATOR_UPDATE_INTERVAL": "5",
			"BUNDLE_SUPER_USERS": getpass.getuser(),
		},
	) as condor:
		yield condor


def negotiator_ad(condor, projection=None):
	"""The negotiator's daemon ad as the collector has it."""
	ads = condor.status(ad_type=htcondor.AdTypes.Negotiator, projection=projection or [])
	assert len(ads) >= 1, "No negotiator ad in the collector"
	return ads[0]


@action
def unfillable_bundle(condor, test_dir, path_to_sleep):
	"""Submit a bundle bigger than the pool, and wait until the negotiator is
	reporting it as in progress."""
	handle = condor.submit(
		description={
			"executable": path_to_sleep,
			"arguments": "600",
			"log": (test_dir / "bundle.log").as_posix(),
			"should_transfer_files": False,
			"Request_Cpus": 1,
			"Request_Memory": 1,
			"Request_Disk": 1,
			"+IsBundle": "true",
		},
		count=BUNDLE_N,
	)

	start = time.time()
	while (time.time() - start) < 180:
		ad = negotiator_ad(condor, CURRENT_STATE_ATTRS)
		if int(ad.get("BundlesInProgress", 0)) >= 1:
			break
		time.sleep(2)

	yield handle
	handle.remove()


@action
def stats(condor, unfillable_bundle):
	"""The negotiator ad, read once the bundle has been in progress long enough
	for a nonzero wait time to be reportable."""
	time.sleep(3)
	return negotiator_ad(condor)


@action
def cycle_history(condor, unfillable_bundle, stats):
	"""The per-cycle bundle statistics, as {index: {attr: value}}.

	The negotiator keeps a ring of recent cycles, so rather than guess which
	index holds a cycle that saw the bundle, collect them all."""
	ad = negotiator_ad(condor)
	history = {}
	for i in range(0, 10):
		row = {}
		for attr in (
			"ActiveBundles",
			"BundleSubmitters",
			"BundleSlotsWanted",
			"BundleMatches",
			"BundleDuration",
		):
			name = f"LastNegotiationCycle{attr}{i}"
			if name in ad:
				row[attr] = int(ad[name])
		if row:
			history[i] = row
	return history


class TestBundleNegotiatorStats:
	def test_current_state_attributes_are_published(self, stats):
		"""All four current-state attributes are on the ad.  They are published
		unconditionally, so a pool with no bundles reports zeros rather than
		leaving an admin's graph with a hole in it."""
		missing = [attr for attr in CURRENT_STATE_ATTRS if attr not in stats]
		assert not missing, f"Negotiator ad is missing {missing}"

	def test_bundle_is_reported_in_progress(self, stats):
		"""The unfillable bundle is counted as being filled."""
		assert int(stats["BundlesInProgress"]) >= 1

	def test_wanted_slots_are_reported(self, stats):
		"""It still wants the slots the pool cannot give it.  The pool has
		NUM_CPUS, so at least BUNDLE_N - NUM_CPUS are outstanding."""
		assert int(stats["BundleSlotsWanted"]) >= BUNDLE_N - NUM_CPUS

	def test_matched_total_counts_the_slots_it_did_get(self, stats):
		"""A partly-filled bundle still matched what the pool had, and the
		lifetime counter records it.  Unlike BundlesInProgress this is not aged
		out, so it answers 'are bundles being used in this pool at all'."""
		assert int(stats["BundleSlotsMatchedTotal"]) >= 1

	def test_wait_time_is_reported(self, stats):
		"""A bundle that cannot fill waits forever; the wait time is the signal
		for that, so it must actually advance rather than sit at zero."""
		assert int(stats["BundleMaxWaitSeconds"]) > 0

	def test_wait_time_climbs(self, condor, stats):
		"""...and keep climbing while the bundle stays unfilled."""
		before = int(stats["BundleMaxWaitSeconds"])
		start = time.time()
		later = before
		while later <= before and (time.time() - start) < 60:
			time.sleep(5)
			later = int(negotiator_ad(condor)["BundleMaxWaitSeconds"])
		assert later > before, (
			f"BundleMaxWaitSeconds stuck at {before} while the bundle was unfilled"
		)

	def test_per_cycle_history_saw_the_bundle(self, cycle_history):
		"""Some cycle in the ring negotiated for the bundle."""
		assert cycle_history, "No per-cycle bundle statistics published at all"
		active = [row.get("ActiveBundles", 0) for row in cycle_history.values()]
		assert max(active) >= 1, (
			f"No cycle in the ring reported an active bundle: {cycle_history}"
		)

	def test_per_cycle_history_saw_the_bundle_submitter(self, cycle_history):
		"""And ran a bundle round for the reserved submitter."""
		submitters = [row.get("BundleSubmitters", 0) for row in cycle_history.values()]
		assert max(submitters) >= 1, (
			f"No cycle in the ring ran a bundle round: {cycle_history}"
		)

	def test_per_cycle_wanted_slots_reported(self, cycle_history):
		"""The per-cycle view reports the unmet demand too."""
		wanted = [row.get("BundleSlotsWanted", 0) for row in cycle_history.values()]
		assert max(wanted) >= 1, (
			f"No cycle reported slots wanted by a bundle: {cycle_history}"
		)
