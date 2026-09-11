#!/usr/bin/env pytest

# Test the MATCH_INFO advance notification for slot bundle matches.
#
# When NEGOTIATOR_INFORM_STARTD_OF_BUNDLE_MATCH is set, the negotiator tells the
# startd about a bundle match ahead of the claim, appending a small ClassAd of
# match metadata (IsBundle, BundleId) after the claim id.  That trailing ad is a
# wire format change, so both ends gate it on the peer's version: the negotiator
# appends it only to a startd new enough to read it, and the startd reads it only
# from a negotiator new enough to have sent it.
#
# Both gates must name the version the feature actually ships in, and must name
# the *same* version.  Getting that wrong does not fail safe: a gate set too low
# means daemons that predate the feature are treated as supporting it, so the
# negotiator appends an ad the old startd never reads, its end_of_message() sees
# untouched bytes and fails, and the match is dropped -- silently, and only in
# pools that enable this off-by-default knob.
#
# This test covers the same-version case, which is what a single build can
# exercise: the ad is sent, read, and parsed, with no read errors on either side.
# The mixed-version cases cannot be tested here; they need two builds.

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
			"BUNDLE_SUPER_USERS": getpass.getuser(),
			# The feature under test; off by default.
			"NEGOTIATOR_INFORM_STARTD_OF_BUNDLE_MATCH": "true",
			"STARTD_DEBUG": "D_FULLDEBUG",
		},
	) as condor:
		yield condor


@action
def bundle_startd_log(condor, test_dir, path_to_sleep):
	"""Submit a bundle, wait for it to fill, and return the startd's log."""
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

	log = condor.startd_log.path.read_text()

	yield {"log": log, "cluster": handle.clusterid}
	handle.remove()


class TestBundleMatchInfo:
	def test_startd_was_told_the_match_is_a_bundle(self, bundle_startd_log):
		"""The startd read the metadata ad and recognized a bundle match.  If
		either version gate named a version the peer does not satisfy, the ad is
		never sent or never read and this line never appears."""
		assert "MATCH_INFO: match is for slot bundle" in bundle_startd_log["log"], (
			"Startd never reported a slot bundle MATCH_INFO"
		)

	def test_startd_got_the_bundle_id(self, bundle_startd_log):
		"""The BundleId survived the trip, so the ad was parsed and not merely
		present."""
		suffix = f"#{bundle_startd_log['cluster']}"
		lines = [
			line for line in bundle_startd_log["log"].splitlines()
			if "match is for slot bundle" in line
		]
		assert lines, "No slot bundle MATCH_INFO lines to check"
		assert all(line.rstrip().endswith(suffix) for line in lines), (
			f"MATCH_INFO carried the wrong BundleId (expected one ending in "
			f"{suffix}): {lines}"
		)

	def test_no_metadata_read_errors(self, bundle_startd_log):
		"""A gate set too low shows up here: the startd tries to read an ad the
		peer never sent."""
		assert "can't read MATCH_INFO metadata ad" not in bundle_startd_log["log"]

	def test_no_end_of_message_errors(self, bundle_startd_log):
		"""And the converse: an ad sent to a startd that does not read it leaves
		untouched bytes, so end_of_message fails and the match is dropped."""
		assert "can't read end of message for MATCH_INFO" not in bundle_startd_log["log"]
