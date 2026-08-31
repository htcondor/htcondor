#!/usr/bin/env pytest

# Test that slot bundle creation and matching works:
# 1. Stand up a personal condor with default partitionable slots
# 2. Create a bundle requesting N slots via the Python API
# 3. Verify the schedd holds N claims for the bundle (BundleNumSatisfied == N)
# 4. Submit a fair-share job that wants ALL the cpus and verify it is forced
#    to wait, because the bundle grabbed its slots ahead of fair share.
#
# Unlike OCUs, a bundle needs no "wanted" job first: the schedd advertises a
# reserved submitter for outstanding bundles on its own, and the negotiator
# matches that submitter first, off-the-books.

import logging
import getpass
import time

import htcondor2 as htcondor
import classad2 as classad

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# Total cpus in the pool and how many the bundle should grab.
NUM_CPUS = 4
BUNDLE_N = 2


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
		},
	) as condor:
		yield condor


@action
def the_bundle(condor):
	"""Create a bundle requesting BUNDLE_N single-cpu slots."""
	schedd = condor.get_local_schedd()

	request = classad.ClassAd()
	request["Owner"] = getpass.getuser()
	request["RequestCpus"] = 1
	request["RequestMemory"] = 1
	request["RequestDisk"] = 1
	request["BundleNumRequested"] = BUNDLE_N

	result = schedd.create_bundle(request)
	logger.debug(f"Bundle create result: {result}")
	assert result["Result"] == 0, f"Failed to create bundle: {result.get('ErrorString', 'unknown')}"

	yield result

	# Cleanup: remove the bundle.
	try:
		remove_ad = classad.ClassAd()
		remove_ad["BundleId"] = result["BundleId"]
		schedd.remove_bundle(result["BundleId"])
	except Exception:
		pass


def bundle_satisfied(condor, bundle_id):
	"""Return the BundleNumSatisfied for the given bundle, or -1 if it is gone."""
	schedd = condor.get_local_schedd()
	for ad in schedd.query_bundle(classad.ClassAd()):
		if ad.get("BundleId") == bundle_id:
			return int(ad.get("BundleNumSatisfied", 0))
	return -1


@action
def satisfied_count(condor, the_bundle):
	"""Poll the schedd until the bundle holds all BUNDLE_N claims."""
	bundle_id = the_bundle["BundleId"]

	start = time.time()
	held = bundle_satisfied(condor, bundle_id)
	while held < BUNDLE_N and (time.time() - start) < 120:
		time.sleep(2)
		held = bundle_satisfied(condor, bundle_id)

	assert held >= BUNDLE_N, (
		f"Bundle {bundle_id} only reached {held} of {BUNDLE_N} held claims"
	)
	return held


def bundle_claimed_slot_names(condor):
	"""Return the set of distinct slot names currently claimed by the reserved
	bundle submitter (RemoteUser like condor_bundle@...)."""
	return {
		ad["Name"]
		for ad in condor.status(
			ad_type=htcondor.AdTypes.Startd,
			constraint='regexp("^condor_bundle@", RemoteUser)',
			projection=["Name", "RemoteUser"],
		)
	}


@action
def bundle_claimed_slots(condor, satisfied_count):
	"""The set of distinct slot names claimed by the reserved bundle submitter.
	Because the claims are held off-the-books, the negotiator must carve a fresh
	slot per request; if it instead re-matched (and preempted) an already-held
	slot, we would see fewer distinct slots than requested.

	Poll rather than query once: BundleNumSatisfied (the schedd's view, which
	satisfied_count gates on) flips to N the instant the schedd records each
	claim, but condor_status reads the collector, whose ad for a freshly carved
	dynamic slot lags the claim by an update cycle.  A single immediate query
	can therefore see the last-claimed slot's ad before it has propagated."""
	start = time.time()
	slots = bundle_claimed_slot_names(condor)
	while len(slots) < BUNDLE_N and (time.time() - start) < 60:
		time.sleep(2)
		slots = bundle_claimed_slot_names(condor)
	return slots


def isbundle_slot_ads(condor):
	"""Return {slot Name: BundleId} for every slot the startd advertises as
	held for a bundle (the first-class IsBundle slot attribute)."""
	return {
		ad["Name"]: ad.get("BundleId")
		for ad in condor.status(
			ad_type=htcondor.AdTypes.Startd,
			constraint="IsBundle == true",
			projection=["Name", "IsBundle", "BundleId"],
		)
	}


@action
def isbundle_slots(condor, the_bundle, bundle_claimed_slots):
	"""Poll for the startd-advertised IsBundle slots.  Depends on
	bundle_claimed_slots so the claims are already held; like that fixture we
	poll to let the freshly carved dynamic-slot ads propagate to the collector."""
	start = time.time()
	slots = isbundle_slot_ads(condor)
	while len(slots) < BUNDLE_N and (time.time() - start) < 60:
		time.sleep(2)
		slots = isbundle_slot_ads(condor)
	return slots


@action
def after_bundle_removed(condor, the_bundle, bundle_claimed_slots, isbundle_slots):
	"""Remove the satisfied bundle and wait for its held claims to be released.
	Depends on bundle_claimed_slots so removal happens only after we have
	confirmed the bundle was holding BUNDLE_N distinct slots."""
	schedd = condor.get_local_schedd()
	bundle_id = the_bundle["BundleId"]

	remove_ad = schedd.remove_bundle(bundle_id)

	# Poll until no slot is claimed by the bundle submitter any more.
	start = time.time()
	remaining = bundle_claimed_slot_names(condor)
	while remaining and (time.time() - start) < 60:
		time.sleep(2)
		remaining = bundle_claimed_slot_names(condor)

	# The bundle should also no longer appear in the schedd's bundle list.
	still_listed = any(
		ad.get("BundleId") == bundle_id
		for ad in schedd.query_bundle(classad.ClassAd())
	)

	return {
		"remove_ad": remove_ad,
		"remaining_slots": remaining,
		"still_listed": still_listed,
	}


class TestBundleBasic:
	def test_bundle_created(self, the_bundle):
		"""Verify the bundle was created successfully."""
		assert the_bundle["Result"] == 0
		assert "BundleId" in the_bundle

	def test_bundle_satisfied(self, satisfied_count):
		"""Verify the schedd held N claims for the bundle."""
		assert satisfied_count >= BUNDLE_N

	def test_bundle_claims_distinct_slots(self, bundle_claimed_slots):
		"""Verify the N held claims are on N distinct slots, not one slot
		claimed (and re-preempted) N times."""
		assert len(bundle_claimed_slots) == BUNDLE_N, (
			f"Expected {BUNDLE_N} distinct bundle-claimed slots, "
			f"got {sorted(bundle_claimed_slots)}"
		)

	def test_slots_advertise_isbundle(self, isbundle_slots, the_bundle):
		"""Verify the startd advertises the first-class IsBundle slot attribute
		on each held slot, tagged with the correct BundleId, so bundle occupancy
		is visible via condor_status (not just an opaque RemoteUser)."""
		assert len(isbundle_slots) == BUNDLE_N, (
			f"Expected {BUNDLE_N} slots advertising IsBundle, "
			f"got {sorted(isbundle_slots)}"
		)
		bundle_id = the_bundle["BundleId"]
		assert all(bid == bundle_id for bid in isbundle_slots.values()), (
			f"IsBundle slots carry wrong BundleId: {isbundle_slots} "
			f"(expected {bundle_id})"
		)

	def test_isbundle_matches_claimed_slots(self, isbundle_slots, bundle_claimed_slots):
		"""The slots advertising IsBundle should be exactly the slots claimed by
		the reserved bundle submitter."""
		assert set(isbundle_slots) == set(bundle_claimed_slots)

	def test_remove_reports_released_claims(self, after_bundle_removed):
		"""Verify remove succeeds and reports it released the held claims."""
		remove_ad = after_bundle_removed["remove_ad"]
		assert remove_ad["Result"] == 0
		assert remove_ad["BundleClaimsReleased"] == BUNDLE_N

	def test_remove_releases_slots(self, after_bundle_removed):
		"""Verify the held slots are actually released back to the pool."""
		assert after_bundle_removed["remaining_slots"] == set(), (
			f"Slots still claimed by bundle after removal: "
			f"{sorted(after_bundle_removed['remaining_slots'])}"
		)

	def test_remove_forgets_bundle(self, after_bundle_removed):
		"""Verify the bundle no longer appears in the schedd's bundle list."""
		assert after_bundle_removed["still_listed"] is False
