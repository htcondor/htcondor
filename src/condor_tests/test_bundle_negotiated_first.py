#!/usr/bin/env pytest

# Test that the negotiator services slot bundle requests before anything else
# in a negotiation cycle.
#
# A slot bundle is only useful once it holds all N of its slots, and it holds
# each claim idle until then, so waiting behind other submitters is not merely
# slow -- it can leave a bundle permanently short while its claims sit idle.
# The negotiator therefore pulls the reserved bundle submitters out of the
# submitter list and negotiates for them in a round of their own ("Phase 3.5"),
# before the floor round and before any accounting group is negotiated.
#
# Sorting bundle submitters to the front of the list is not enough on its own:
# the floor round runs before the main round, and under HGQ each group is
# negotiated in turn.  Both of those happen outside the sorted list, which is
# why this test asserts on cycle-level ordering and not just on the sort.
#
# Sequence:
#   1. Stand up a personal condor with a verbose negotiator.
#   2. Submit ordinary jobs and a bundle, so a cycle has both to negotiate.
#   3. Wait for the bundle to fill.
#   4. Read the negotiator log and check, within a single negotiation cycle,
#      that the bundle round ran before every other round in that cycle.

import logging
import getpass
import re
import time

import htcondor2 as htcondor
import classad2 as classad

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

NUM_CPUS = 6
BUNDLE_N = 2
OTHER_JOBS = 3


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
			# So the negotiator logs its per-round phase banners.
			"NEGOTIATOR_DEBUG": "D_FULLDEBUG",
		},
	) as condor:
		yield condor


@action
def ordinary_jobs(condor, test_dir, path_to_sleep):
	"""Ordinary jobs, so the cycle has a real submitter to negotiate for
	besides the bundle."""
	handle = condor.submit(
		description={
			"executable": path_to_sleep,
			"arguments": "600",
			"log": (test_dir / "ordinary.log").as_posix(),
			"should_transfer_files": False,
			"Request_Cpus": 1,
			"Request_Memory": 1,
			"Request_Disk": 1,
		},
		count=OTHER_JOBS,
	)
	yield handle
	handle.remove()


@action
def the_bundle(condor, test_dir, path_to_sleep, ordinary_jobs):
	"""Submit a bundle and wait for it to fill."""
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

	schedd = condor.get_local_schedd()
	start = time.time()
	held = 0
	while held < BUNDLE_N and (time.time() - start) < 180:
		time.sleep(2)
		for ad in schedd.query(
			constraint=f"ClusterId == {handle.clusterid}",
			projection=["ClusterId", "BundleNumSatisfied"],
		):
			held = int(ad.get("BundleNumSatisfied", 0))
			break
	assert held >= BUNDLE_N, f"Bundle never filled (reached {held} of {BUNDLE_N})"

	yield handle
	handle.remove()


@action
def bundle_cycle(condor, the_bundle):
	"""The text of one negotiation cycle that included a bundle round.

	Cycles are delimited in the log by the negotiator's own start/finish
	banners, so slicing on them keeps us from comparing a Phase 3.5 in one
	cycle against a Phase 4 in the next."""
	log = condor.negotiator_log.path.read_text()

	cycles = re.split(r"-+ Started Negotiation Cycle -+", log)
	for cycle in cycles[1:]:
		end = cycle.find("Finished Negotiation Cycle")
		if end < 0:
			# An incomplete trailing cycle; ordering in it is not yet decided.
			continue
		body = cycle[:end]
		if "Phase 3.5" in body:
			return body

	raise AssertionError(
		"No complete negotiation cycle in the log ran a slot bundle round"
	)


@action
def submitter_ads(condor, the_bundle):
	"""Every submitter ad the schedd is advertising, by name.

	The schedd builds these by reusing a single ad, so an attribute set for one
	submitter and not cleared for the next silently leaks along the iteration
	order."""
	ads = {}
	start = time.time()
	while getpass.getuser() not in str(ads) and (time.time() - start) < 60:
		ads = {
			ad["Name"]: ad
			for ad in condor.status(
				ad_type=htcondor.AdTypes.Submitter,
				projection=["Name", "IsBundleSubmitter", "IdleJobs"],
			)
		}
		if not ads:
			time.sleep(2)
	return ads


class TestBundleNegotiatedFirst:
	def test_bundle_round_ran(self, bundle_cycle):
		"""The negotiator ran a dedicated round for the bundle submitter."""
		assert "Negotiating slot bundles" in bundle_cycle

	def test_bundle_round_precedes_all_other_negotiation(self, bundle_cycle):
		"""Every 'Phase 4' (negotiating with schedds) in the cycle comes after
		the bundle round -- including the one belonging to the bundle round
		itself, which is the first."""
		bundle_at = bundle_cycle.index("Phase 3.5")
		first_negotiation = bundle_cycle.index("Phase 4.")
		assert bundle_at < first_negotiation, (
			"A round of negotiation ran before the slot bundle round"
		)

	def test_bundle_round_precedes_the_floor_round(self, bundle_cycle):
		"""The floor round runs ahead of the main round, so it is the one
		ordering the submitter sort could never fix.  If this cycle ran one, it
		must still come after the bundle round."""
		floor_at = bundle_cycle.find("running a floor round")
		if floor_at < 0:
			# No submitter had a floor below its usage this cycle.
			return
		assert bundle_cycle.index("Phase 3.5") < floor_at, (
			"The floor round ran before the slot bundle round"
		)

	def test_bundle_submitter_left_out_of_the_ordinary_rounds(self, bundle_cycle):
		"""The bundle submitter is extracted from the submitter list, not just
		sorted to the front, so it is negotiated exactly once -- in its own
		round.  Anything else means it also entered a group or floor round."""
		assert bundle_cycle.count("Negotiating slot bundles") == 1

	def test_only_the_bundle_submitter_is_in_the_bundle_round(self, bundle_cycle):
		"""Exactly one submitter -- the reserved bundle submitter -- belongs in
		the bundle round.  More than that means an ordinary submitter was
		mistaken for a bundle submitter and got the bundle's off-the-books
		treatment: the whole pie and no ceiling."""
		counts = re.findall(r"Negotiating slot bundles for (\d+) submitter", bundle_cycle)
		assert counts == ["1"], (
			f"Bundle round ran for {counts} submitter(s), expected exactly one"
		)

	def test_ordinary_submitter_is_not_marked_as_a_bundle_submitter(self, submitter_ads):
		"""IsBundleSubmitter belongs only to the reserved bundle submitter.  The
		schedd advertises submitters from one reused ad, in name order, so a
		marker left set on the way past 'condor_bundle' would appear on every
		submitter that sorts after it -- including this test's user."""
		mismarked = {
			name: ad.get("IsBundleSubmitter")
			for name, ad in submitter_ads.items()
			if ad.get("IsBundleSubmitter") and not name.startswith("condor_bundle@")
		}
		assert not mismarked, (
			f"Ordinary submitters carry IsBundleSubmitter: {mismarked}"
		)

	def test_the_bundle_submitter_is_marked(self, submitter_ads):
		"""The converse: clearing the marker must not have cleared it for the
		submitter it belongs to, or nothing would be negotiated first."""
		bundle = [n for n in submitter_ads if n.startswith("condor_bundle@")]
		assert bundle, f"No bundle submitter ad advertised: {sorted(submitter_ads)}"
		for name in bundle:
			assert submitter_ads[name].get("IsBundleSubmitter"), (
				f"Bundle submitter {name} is not marked with IsBundleSubmitter"
			)
