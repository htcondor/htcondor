#!/usr/bin/env pytest

# Test that one user may hold several slot bundles at once, and that the
# schedd publishes the ordering key the negotiator uses to keep bundles in a
# stable order.
#
# There used to be a one-bundle-per-user rule, refused at submit time.  It is
# gone: the gate on bundles is now a single schedd-wide switch
# (ENABLE_SLOT_BUNDLES), so a user of a schedd with bundles turned on may have
# as many as they like.
#
# 1. Stand up a personal condor big enough for two bundles at once
# 2. Submit two separate clusters of "+IsBundle = true" jobs as the same user
# 3. Verify both bundles fill, on disjoint sets of slots, each tagged with its
#    own BundleId, and that the jobs of both then run
# 4. Submit one more bundle than the pool can satisfy, and verify the reserved
#    bundle submitter ad carries BundleOldestQDate naming that still-waiting
#    bundle -- the key tracks the oldest bundle that still wants slots, not the
#    oldest bundle -- and that no other submitter ad has it.  The schedd builds
#    submitter ads by reusing one ad, so an attribute set for the bundle
#    submitter and not cleared leaks to everyone advertised after it.
#
# BundleOldestQDate is what lets the negotiator sort bundle submitters the same
# way in every cycle.  Two bundles served in a different order on different
# cycles can each take a partial fill and deadlock, since a partial fill is
# held until its bundle completes.  This test covers the schedd half of that
# (computing and publishing the key); the negotiator half -- the comparator --
# needs two schedds in one pool to observe, which is beyond this fixture.

import logging
import time

import htcondor2 as htcondor

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# Enough cpus for both bundles to be satisfied at the same time.
NUM_CPUS = 4
BUNDLE_N = 2
NUM_BUNDLES = 2


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
			# Slot bundles are off unless the admin turns them on.  This is the
			# whole gate now -- there is no per-user allow list.
			"ENABLE_SLOT_BUNDLES": "true",
		},
	) as condor:
		yield condor


def submit_bundle(condor, test_dir, path_to_sleep, tag):
	return condor.submit(
		description={
			"executable": path_to_sleep,
			"arguments": "300",
			"log": (test_dir / f"bundle_{tag}.log").as_posix(),
			"should_transfer_files": False,
			"Request_Cpus": 1,
			"Request_Memory": 1,
			"Request_Disk": 1,
			"+IsBundle": "true",
		},
		count=BUNDLE_N,
	)


@action
def bundles(condor, test_dir, path_to_sleep):
	"""Two bundles, submitted back to back by the same user.  Under the old
	one-bundle-per-user rule the second submit was refused outright.  Together
	they want exactly as many slots as the pool has."""
	handles = []
	for tag in range(NUM_BUNDLES):
		handles.append(submit_bundle(condor, test_dir, path_to_sleep, tag))
		# Make sure the clusters get distinct QDates, so the test can tell
		# which one the ordering key should name.
		time.sleep(1)

	yield handles

	for handle in handles:
		handle.remove()


@action
def waiting_bundle(condor, test_dir, path_to_sleep, satisfied_counts):
	"""A third bundle, submitted once the first two have taken every slot in
	the pool, so that it stays unsatisfied for the life of the test.

	An outstanding bundle is what makes BundleOldestQDate hold still long
	enough to assert on: the schedd publishes the key only while some bundle is
	still waiting for slots.  Because this one is the *newest* bundle but the
	only waiting one, it also shows the key tracks the oldest bundle that still
	wants slots rather than simply the oldest bundle."""
	time.sleep(1)
	handle = submit_bundle(condor, test_dir, path_to_sleep, "waiting")

	yield handle

	handle.remove()


def bundle_satisfied(condor, cluster):
	"""BundleNumSatisfied from the cluster ad, or -1 if the cluster is gone."""
	schedd = condor.get_local_schedd()
	for ad in schedd.query(
		constraint=f"ClusterId == {cluster}",
		projection=["ClusterId", "BundleNumSatisfied"],
	):
		return int(ad.get("BundleNumSatisfied", 0))
	return -1


@action
def satisfied_counts(condor, bundles):
	"""Poll until every bundle holds all of its claims."""
	clusters = [h.clusterid for h in bundles]

	def counts():
		return [bundle_satisfied(condor, c) for c in clusters]

	start = time.time()
	held = counts()
	while min(held) < BUNDLE_N and (time.time() - start) < 180:
		time.sleep(2)
		held = counts()

	assert min(held) >= BUNDLE_N, (
		f"Not every bundle filled: clusters {clusters} reached {held} "
		f"of {BUNDLE_N} claims each"
	)
	return dict(zip(clusters, held))


def isbundle_slot_ads(condor):
	"""{slot Name: BundleId} for every slot a startd says is held for a
	bundle."""
	return {
		ad["Name"]: str(ad.get("BundleId"))
		for ad in condor.status(
			ad_type=htcondor.AdTypes.Startd,
			constraint="IsBundle == true",
			projection=["Name", "IsBundle", "BundleId"],
		)
	}


@action
def slots_by_bundle(condor, satisfied_counts):
	"""{BundleId: {slot names}} once every bundle's slots are visible in the
	collector.  Poll rather than query once: the schedd records a claim before
	the collector has the freshly carved dynamic slot's ad."""
	want = NUM_BUNDLES * BUNDLE_N

	def grouped():
		by_bundle = {}
		for slot, bundle_id in isbundle_slot_ads(condor).items():
			by_bundle.setdefault(bundle_id, set()).add(slot)
		return by_bundle

	start = time.time()
	by_bundle = grouped()
	while sum(len(s) for s in by_bundle.values()) < want and (time.time() - start) < 60:
		time.sleep(2)
		by_bundle = grouped()
	return by_bundle


@action
def cluster_qdates(condor, bundles, waiting_bundle):
	"""The QDate of every bundle cluster's ad, which is the ordering key."""
	schedd = condor.get_local_schedd()
	qdates = {}
	for handle in list(bundles) + [waiting_bundle]:
		for ad in schedd.query(
			constraint=f"ClusterId == {handle.clusterid}",
			projection=["ClusterId", "QDate"],
		):
			qdates[handle.clusterid] = int(ad["QDate"])
			break
	return qdates


@action
def submitter_ads(condor, waiting_bundle):
	"""Every submitter ad the schedd advertises, by name."""
	def marked_ads():
		return {
			ad["Name"]: ad
			for ad in condor.status(
				ad_type=htcondor.AdTypes.Submitter,
				projection=["Name", "IsBundleSubmitter", "BundleOldestQDate"],
			)
		}

	# Wait for the schedd to advertise the key for the still-waiting bundle.
	start = time.time()
	ads = marked_ads()
	while (time.time() - start) < 120 and not any(
		ad.get("BundleOldestQDate") for ad in ads.values()
	):
		time.sleep(2)
		ads = marked_ads()
	return ads


@action
def running_jobs(condor, bundles, satisfied_counts):
	"""Every bundle is complete, so all of their jobs may run."""
	for handle in bundles:
		assert handle.wait(
			condition=ClusterState.all_running,
			timeout=180,
			fail_condition=ClusterState.any_held,
		), f"Jobs of bundle cluster {handle.clusterid} never all started running"
	return True


class TestBundleMultiple:
	def test_all_bundles_submitted(self, bundles):
		"""A user may submit more than one bundle; none of these is refused."""
		assert len(bundles) == NUM_BUNDLES
		clusters = {h.clusterid for h in bundles}
		assert len(clusters) == NUM_BUNDLES, (
			f"Expected {NUM_BUNDLES} distinct bundle clusters, got {clusters}"
		)

	def test_all_bundles_filled(self, satisfied_counts):
		"""Both bundles reach their full complement of claims; neither is
		stuck part-filled waiting on slots the other is holding."""
		assert all(n >= BUNDLE_N for n in satisfied_counts.values()), (
			f"Some bundle did not fill: {satisfied_counts}"
		)

	def test_bundles_hold_disjoint_slots(self, slots_by_bundle):
		"""Each bundle holds its own slots: N distinct slots per bundle, and no
		slot claimed for two bundles."""
		assert len(slots_by_bundle) == NUM_BUNDLES, (
			f"Expected {NUM_BUNDLES} distinct BundleIds on held slots, "
			f"got {sorted(slots_by_bundle)}"
		)
		for bundle_id, slots in slots_by_bundle.items():
			assert len(slots) == BUNDLE_N, (
				f"Bundle {bundle_id} holds {sorted(slots)}, "
				f"expected {BUNDLE_N} distinct slots"
			)
		all_slots = [s for slots in slots_by_bundle.values() for s in slots]
		assert len(all_slots) == len(set(all_slots)), (
			f"A slot is claimed for more than one bundle: {sorted(all_slots)}"
		)

	def test_jobs_of_every_bundle_run(self, running_jobs):
		"""Each bundle releases its own jobs once it is complete."""
		assert running_jobs

	def test_bundle_submitter_publishes_ordering_key(self, submitter_ads, cluster_qdates, waiting_bundle):
		"""The reserved bundle submitter advertises BundleOldestQDate, and it
		names the oldest bundle that still wants slots -- here the newest
		bundle, because it is the only one still waiting.  The negotiator sorts
		bundle submitters on this so bundles are served in the same order every
		cycle."""
		marked = [
			ad for ad in submitter_ads.values()
			if ad.get("IsBundleSubmitter")
		]
		assert len(marked) == 1, (
			f"Expected exactly one bundle submitter ad, got "
			f"{[ad.get('Name') for ad in marked]}"
		)
		published = marked[0].get("BundleOldestQDate")
		assert published is not None, (
			"Bundle submitter ad has no BundleOldestQDate; the negotiator has "
			"no stable key to order bundles by"
		)
		expected = cluster_qdates[waiting_bundle.clusterid]
		assert int(published) == expected, (
			f"BundleOldestQDate is {published}, expected {expected} -- the "
			f"QDate of the only bundle still waiting for slots "
			f"(cluster {waiting_bundle.clusterid}), from {cluster_qdates}"
		)

	def test_ordering_key_does_not_leak_to_other_submitters(self, submitter_ads):
		"""The schedd fills every submitter ad from one reused ClassAd, so an
		attribute set for the bundle submitter and not cleared leaks into every
		submitter advertised after it."""
		leaked = [
			name for name, ad in submitter_ads.items()
			if not ad.get("IsBundleSubmitter") and ad.get("BundleOldestQDate") is not None
		]
		assert leaked == [], (
			f"BundleOldestQDate leaked onto non-bundle submitter ads: {leaked}"
		)
