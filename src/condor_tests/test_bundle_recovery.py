#!/usr/bin/env pytest

# Test that a slot bundle recovers when one of its held claims is lost.
#
# This exercises the schedd's bundle claim accounting:
#   - num_satisfied is decremented when a held bundle claim goes away
#     (unlinkMrec), and
#   - the still-incomplete bundle is re-injected into the next negotiation
#     cycle and re-matched, driving BundleNumSatisfied back up to N.
#
# Without the decrement, num_satisfied would stay pinned at N forever and the
# bundle would silently run one slot short with no re-request.
#
# Sequence:
#   1. Stand up a personal condor with NUM_CPUS partitionable slots.
#   2. Create a bundle for BUNDLE_N single-cpu slots and wait until satisfied.
#   3. condor_vacate one of the held slots, forcing the startd to release that
#      claim (RELEASE_CLAIM back to the schedd).
#   4. Verify BundleNumSatisfied dips below N and then recovers back to N.

import logging
import getpass
import time

import htcondor2 as htcondor
import classad2 as classad

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

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


def bundle_satisfied(condor, bundle_id):
	"""BundleNumSatisfied for the given bundle, or -1 if it is gone."""
	schedd = condor.get_local_schedd()
	for ad in schedd.query_bundle(classad.ClassAd()):
		if ad.get("BundleId") == bundle_id:
			return int(ad.get("BundleNumSatisfied", 0))
	return -1


def wait_for_satisfied(condor, bundle_id, target, timeout=120):
	"""Poll until BundleNumSatisfied reaches target (or timeout)."""
	start = time.time()
	held = bundle_satisfied(condor, bundle_id)
	while held < target and (time.time() - start) < timeout:
		time.sleep(2)
		held = bundle_satisfied(condor, bundle_id)
	return held


def bundle_claimed_slot_names(condor):
	"""Distinct slot names currently claimed by the reserved bundle submitter."""
	return {
		ad["Name"]
		for ad in condor.status(
			ad_type=htcondor.AdTypes.Startd,
			constraint='regexp("^condor_bundle@", RemoteUser)',
			projection=["Name", "RemoteUser"],
		)
	}


@action
def the_bundle(condor):
	"""Create a bundle requesting BUNDLE_N single-cpu slots and wait for it to fill."""
	schedd = condor.get_local_schedd()

	request = classad.ClassAd()
	request["Owner"] = getpass.getuser()
	request["RequestCpus"] = 1
	request["RequestMemory"] = 1
	request["RequestDisk"] = 1
	request["BundleNumRequested"] = BUNDLE_N

	result = schedd.create_bundle(request)
	assert result["Result"] == 0, f"Failed to create bundle: {result.get('ErrorString', 'unknown')}"

	held = wait_for_satisfied(condor, result["BundleId"], BUNDLE_N)
	assert held >= BUNDLE_N, f"Bundle never reached {BUNDLE_N} claims (got {held})"

	yield result

	try:
		schedd.remove_bundle(result["BundleId"])
	except Exception:
		pass


@action
def recovery(condor, the_bundle):
	"""Vacate one held bundle slot, then verify the bundle re-requests and
	recovers back to BUNDLE_N held claims."""
	bundle_id = the_bundle["BundleId"]

	# Wait for the collector to show the held slots, then pick one to vacate.
	start = time.time()
	slots = bundle_claimed_slot_names(condor)
	while len(slots) < BUNDLE_N and (time.time() - start) < 60:
		time.sleep(2)
		slots = bundle_claimed_slot_names(condor)
	assert len(slots) >= 1, "No bundle-claimed slots visible to vacate"

	victim = sorted(slots)[0]
	logger.debug(f"Vacating bundle-held slot {victim}")
	vacate = condor.run_command(["condor_vacate", "-fast", victim])
	assert vacate.returncode == 0, f"condor_vacate failed: {vacate.stderr}"

	# The bundle should first drop below N as the claim is released...
	start = time.time()
	dipped = False
	while (time.time() - start) < 60:
		if bundle_satisfied(condor, bundle_id) < BUNDLE_N:
			dipped = True
			break
		time.sleep(1)

	# ...and then recover back to N once the schedd re-requests and re-matches.
	recovered = wait_for_satisfied(condor, bundle_id, BUNDLE_N, timeout=120)

	return {"dipped": dipped, "recovered": recovered}


class TestBundleRecovery:
	def test_bundle_recovers_after_claim_loss(self, recovery):
		"""After a held claim is lost, the bundle re-fills to BUNDLE_N."""
		assert recovery["recovered"] >= BUNDLE_N, (
			f"Bundle did not recover to {BUNDLE_N} after losing a claim "
			f"(reached {recovery['recovered']})"
		)
