#!/usr/bin/env pytest

# Test that slot bundles work, end to end, from a normal job submission:
# 1. Stand up a personal condor with default partitionable slots
# 2. Submit a cluster of BUNDLE_N jobs marked "+IsBundle = true"
# 3. Verify the schedd grabs BUNDLE_N distinct slots and holds them
#    (BundleNumSatisfied on the cluster ad reaches N)
# 4. Verify the startd advertises IsBundle/BundleId on those slots
# 5. Verify the jobs then run on those slots, and that the claims are
#    released when the jobs finish -- a bundle lives exactly as long as
#    its jobs do.
#
# There is no explicit bundle create step: a cluster of IsBundle jobs *is*
# the bundle.  N is the number of jobs in the cluster.

import logging
import getpass
import time

import htcondor2 as htcondor
import classad2 as classad

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# Total cpus in the pool and how many of them the bundle should grab.
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
			# Creating a slot bundle is privileged.
			"BUNDLE_SUPER_USERS": getpass.getuser(),
		},
	) as condor:
		yield condor


@action
def bundle_handle(condor, test_dir, path_to_sleep):
	"""Submit a cluster of BUNDLE_N jobs marked as a slot bundle.  The jobs
	sleep long enough that we can observe them holding their slots."""
	handle = condor.submit(
		description={
			"executable": path_to_sleep,
			"arguments": "60",
			"log": (test_dir / "bundle_job.log").as_posix(),
			"should_transfer_files": False,
			"Request_Cpus": 1,
			"Request_Memory": 1,
			"Request_Disk": 1,
			"+IsBundle": "true",
		},
		count=BUNDLE_N,
	)

	yield handle
	handle.remove()


def bundle_satisfied(condor, cluster):
	"""BundleNumSatisfied as published by the schedd on the cluster ad, or -1
	if the cluster is gone."""
	schedd = condor.get_local_schedd()
	ads = schedd.query(
		constraint=f"ClusterId == {cluster}",
		projection=["ClusterId", "BundleNumRequested", "BundleNumSatisfied"],
	)
	for ad in ads:
		return int(ad.get("BundleNumSatisfied", 0))
	return -1


@action
def satisfied_count(condor, bundle_handle):
	"""Poll the schedd until the bundle holds all BUNDLE_N claims."""
	cluster = bundle_handle.clusterid

	start = time.time()
	held = bundle_satisfied(condor, cluster)
	while held < BUNDLE_N and (time.time() - start) < 120:
		time.sleep(2)
		held = bundle_satisfied(condor, cluster)

	assert held >= BUNDLE_N, (
		f"Bundle for cluster {cluster} only reached {held} of {BUNDLE_N} held claims"
	)
	return held


def bundle_claimed_slot_names(condor):
	"""Return the set of distinct slot names the startds say are held for a
	bundle.  (RemoteUser is condor_bundle only while the claim sits idle; once
	the bundle is complete and its jobs start, the slot's RemoteUser becomes
	the job owner, but IsBundle stays set for the life of the claim.)"""
	return set(isbundle_slot_ads(condor).keys())


@action
def bundle_claimed_slots(condor, satisfied_count):
	"""The set of distinct slot names the bundle is holding.
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
def isbundle_slots(condor, bundle_claimed_slots):
	"""The {slot: BundleId} map for the slots the bundle holds, taken once the
	bundle_claimed_slots fixture has confirmed they are all visible."""
	return isbundle_slot_ads(condor)


@action
def running_jobs(condor, bundle_handle, satisfied_count):
	"""Once the bundle is full its jobs are allowed to run.  Wait for all
	BUNDLE_N of them to start."""
	assert bundle_handle.wait(
		condition=ClusterState.all_running,
		timeout=180,
		fail_condition=ClusterState.any_held,
	), "Bundle jobs never all started running"
	return True


@action
def after_jobs_done(condor, bundle_handle, running_jobs, bundle_claimed_slots, isbundle_slots):
	"""A bundle has the lifetime of its jobs: once they complete, the held
	claims should be released back to the pool."""
	assert bundle_handle.wait(
		condition=ClusterState.all_complete,
		timeout=300,
	), "Bundle jobs never completed"

	start = time.time()
	remaining = bundle_claimed_slot_names(condor)
	while remaining and (time.time() - start) < 120:
		time.sleep(2)
		remaining = bundle_claimed_slot_names(condor)

	return remaining


class TestBundleBasic:
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

	def test_slots_advertise_isbundle(self, isbundle_slots, bundle_handle):
		"""Verify the startd advertises the first-class IsBundle slot attribute
		on each held slot, tagged with the bundle's id, so bundle occupancy is
		visible via condor_status (not just an opaque RemoteUser)."""
		assert len(isbundle_slots) == BUNDLE_N, (
			f"Expected {BUNDLE_N} slots advertising IsBundle, "
			f"got {sorted(isbundle_slots)}"
		)
		# The bundle id ends in the cluster id of the bundle's jobs.
		suffix = f"#{bundle_handle.clusterid}"
		assert all(str(bid).endswith(suffix) for bid in isbundle_slots.values()), (
			f"IsBundle slots carry wrong BundleId: {isbundle_slots} "
			f"(expected one ending in {suffix})"
		)

	def test_jobs_run_on_bundle_slots(self, running_jobs):
		"""Once the bundle is complete, its jobs run on the slots it holds."""
		assert running_jobs

	def test_claims_released_when_jobs_done(self, after_jobs_done):
		"""Verify the held slots go back to the pool when the jobs finish."""
		assert after_jobs_done == set(), (
			f"Slots still claimed by the bundle after its jobs finished: "
			f"{sorted(after_jobs_done)}"
		)
