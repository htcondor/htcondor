#!/usr/bin/env pytest

# Test the "one slot bundle per user" restriction.
#
# A slot bundle grabs slots off-the-books ahead of fair share and holds them
# until all of its jobs have run, so a user may only have one at a time.  A
# second one is refused at submit time, with an error naming the bundle the
# user already has.
#
# Sequence:
#   1. Stand up a personal condor.
#   2. Submit a bundle; it succeeds.
#   3. Submit a second bundle; condor_submit must fail with a clear error.
#   4. An ordinary (non-bundle) submit is unaffected and still succeeds.
#   5. Remove the first bundle; once the schedd notices, a new bundle can be
#      submitted again.

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
			# Creating a slot bundle is privileged.
			"BUNDLE_SUPER_USERS": getpass.getuser(),
			# So we can see the schedd report the knob it parsed.
			"SCHEDD_DEBUG": "D_FULLDEBUG",
		},
	) as condor:
		yield condor


def submit_file(test_dir, name, path_to_sleep, is_bundle):
	"""Write a submit file for a small cluster, optionally a slot bundle."""
	path = test_dir / name
	lines = [
		f"executable = {path_to_sleep}",
		"arguments = 600",
		f"log = {(test_dir / (name + '.log')).as_posix()}",
		"should_transfer_files = false",
		"request_cpus = 1",
		"request_memory = 1",
		"request_disk = 1",
	]
	if is_bundle:
		lines.append("+IsBundle = true")
	lines.append(f"queue {BUNDLE_N}")
	path.write_text("\n".join(lines) + "\n")
	return path


@action
def first_bundle(condor, test_dir, path_to_sleep):
	"""Submit the user's one allowed bundle."""
	path = submit_file(test_dir, "bundle1.sub", path_to_sleep, is_bundle=True)
	result = condor.run_command(["condor_submit", str(path)])
	assert result.returncode == 0, f"First bundle submit failed: {result.stderr}"

	yield result

	condor.run_command(["condor_rm", "-all"])


@action
def second_bundle(condor, test_dir, path_to_sleep, first_bundle):
	"""Try to submit a second bundle as the same user; this must be refused."""
	path = submit_file(test_dir, "bundle2.sub", path_to_sleep, is_bundle=True)
	return condor.run_command(["condor_submit", str(path)])


@action
def ordinary_submit(condor, test_dir, path_to_sleep, first_bundle):
	"""An ordinary submit by a user who already has a bundle is unaffected."""
	path = submit_file(test_dir, "plain.sub", path_to_sleep, is_bundle=False)
	return condor.run_command(["condor_submit", str(path)])


@action
def bundle_after_removal(condor, test_dir, path_to_sleep, second_bundle, ordinary_submit):
	"""Remove everything, then submit a bundle again.  Once the schedd's next
	job-queue census notices the first bundle's jobs are gone, the user is
	allowed a new one, so retry until the schedd catches up."""
	condor.run_command(["condor_rm", "-all"])

	path = submit_file(test_dir, "bundle3.sub", path_to_sleep, is_bundle=True)
	start = time.time()
	result = condor.run_command(["condor_submit", str(path)])
	while result.returncode != 0 and (time.time() - start) < 120:
		time.sleep(5)
		result = condor.run_command(["condor_submit", str(path)])
	return result


@action
def qedit_second_bundle(condor, test_dir, path_to_sleep, first_bundle):
	"""Try to make a second bundle out of a cluster that is already in the
	queue, rather than by submitting one.

	A bundle is just a job attribute, so condor_qedit can create one too.  That
	path commits a transaction with no *new* job ads in it, which is a different
	code path from submit -- and the one that originally slipped past this
	check."""
	path = submit_file(test_dir, "plain_for_qedit.sub", path_to_sleep, is_bundle=False)
	submitted = condor.run_command(["condor_submit", str(path)])
	assert submitted.returncode == 0, f"Setup submit failed: {submitted.stderr}"

	# Find the cluster we just made; it is the highest one in the queue.
	schedd = condor.get_local_schedd()
	clusters = {int(ad["ClusterId"]) for ad in schedd.query(projection=["ClusterId"])}
	assert clusters, "No jobs in the queue to edit"
	cluster = max(clusters)

	result = condor.run_command(
		["condor_qedit", str(cluster), "IsBundle", "true"]
	)
	return {"result": result, "cluster": cluster}


class TestBundleOnePerUser:
	def test_schedd_read_the_bundle_super_users_knob(self, condor, first_bundle):
		"""The schedd reads BUNDLE_SUPER_USERS -- its own knob, not OCU's --
		and reports the list it parsed.  This catches the knob being renamed or
		misspelled; it cannot check the deny path, because the user running a
		personal condor is always a queue super user and so may create bundles
		whatever this list says."""
		log = condor.schedd_log.path.read_text()
		assert "Slot Bundle Super Users:" in log, (
			"Schedd never reported parsing BUNDLE_SUPER_USERS"
		)

	def test_first_bundle_accepted(self, first_bundle):
		"""The user's first bundle submits normally."""
		assert first_bundle.returncode == 0

	def test_second_bundle_refused(self, second_bundle):
		"""A second bundle for the same user is an error."""
		assert second_bundle.returncode != 0, (
			"Second slot bundle was accepted; it should have been refused"
		)

	def test_second_bundle_error_explains_why(self, second_bundle):
		"""The error names the restriction, so the user knows what to do."""
		message = (second_bundle.stderr or "") + (second_bundle.stdout or "")
		assert "one slot bundle per user" in message, (
			f"Unhelpful error for a second bundle: {message}"
		)

	def test_ordinary_submit_still_works(self, ordinary_submit):
		"""The restriction applies only to bundles, not to the user's
		ordinary jobs."""
		assert ordinary_submit.returncode == 0, (
			f"Ordinary submit was refused: {ordinary_submit.stderr}"
		)

	def test_bundle_allowed_again_after_removal(self, bundle_after_removal):
		"""Once the first bundle is gone, the user may have another."""
		assert bundle_after_removal.returncode == 0, (
			f"Could not submit a new bundle after removing the first: "
			f"{bundle_after_removal.stderr}"
		)

	def test_qedit_cannot_create_a_second_bundle(self, qedit_second_bundle):
		"""The one-per-user rule holds however a bundle is created, not just at
		submit.  A transaction that edits existing jobs never reaches the submit
		time checks, so this is its own path."""
		assert qedit_second_bundle["result"].returncode != 0, (
			"condor_qedit created a second slot bundle for a user who already "
			"had one"
		)

	def test_qedit_did_not_set_the_attribute(self, condor, qedit_second_bundle):
		"""The whole transaction is refused, so the edit leaves no trace -- the
		cluster must not come back marked as a bundle."""
		schedd = condor.get_local_schedd()
		ads = schedd.query(
			constraint=f"ClusterId == {qedit_second_bundle['cluster']}",
			projection=["ClusterId", "IsBundle"],
		)
		marked = [ad for ad in ads if ad.get("IsBundle")]
		assert not marked, (
			f"Cluster {qedit_second_bundle['cluster']} was marked IsBundle even "
			f"though the transaction was refused"
		)

	def test_schedd_logged_why_it_refused(self, condor, qedit_second_bundle):
		"""condor_qedit reports only a generic 'Queue transaction failed', because
		it has no error-stack plumbing for any schedd-side check -- so the reason
		is only available in the schedd log.  Assert it is at least recorded
		there, since that is the sole diagnostic an admin gets for this path."""
		log = condor.schedd_log.path.read_text()
		assert "Refusing slot bundle cluster" in log, (
			"Schedd refused the qedit without logging why"
		)
