#!/usr/bin/env pytest

# OCU lifecycle and administration.  Each class gets its own personal condor.
# (Preemption policy is covered by test_ocu_isolation; this runs with
# HTCondor's default policy.  Claim loss is in test_ocu_claim_loss.)
#
# TestOCULifecycle:
#   * only OCU super users may create OCUs; an ordinary user (bob, via the
#     htcondor CLI) is refused, and nothing is created.
#   * an OCU for an unknown user is refused.
#   * an OCU created for a user with no jobs in the queue gets claimed,
#     and stays claimed.
#   * removing an OCU releases its claim: the slot stops being an OCU
#     slot, and other users' jobs can then use those resources.
#   * removing an OCU that doesn't exist is reported as a failure.
#
# TestOCUScheddRestart:
#   * a schedd restart doesn't strand the OCU's claim on the startd (the
#     old schedd releases it, or the new one reuses it),
#   * the OCU is re-claimed after the restart even though its owner has
#     no jobs in the queue,
#   * and there is never more than one slot claimed for the one OCU.

import logging
import time

from ornithology import *

from ocu_helpers import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@config(params={DEFAULT_PREEMPTION: DEFAULT_PREEMPTION})
def preemption(request):
	return request.param


@standup
def condor(test_dir, preemption):
	config = base_config(preemption)
	# Two cpus: one for the OCU, one spare.  Once the OCU is removed, a
	# 2-cpu job can only run on what the OCU released.
	config["NUM_CPUS"] = "2"
	with Condor(test_dir / "condor", config=config) as condor:
		yield condor


@action
def users(condor, test_dir):
	return {u: User(condor, test_dir, u) for u in ("alice", "bob")}


@action
def alice_known(condor, test_dir, users, path_to_sleep):
	"""alice's only job: makes alice a known user; it runs and leaves the
	queue before any OCU is created."""
	cluster = users["alice"].submit(sleep_job(path_to_sleep, test_dir, 1,
		log_name="alice_first.log", extra={"leave_in_queue": "False"}))
	schedd = condor.get_local_schedd()
	wait_for(lambda: len(schedd.query(constraint=f"ClusterId == {cluster}")) == 0,
		timeout=120, what="alice's first job to leave the queue")
	set_priority_factor(condor, "alice", 1000000)
	return cluster


@action
def bob_create_attempt(condor, test_dir, users, alice_known):
	req = test_dir / "bob_ocu_request.txt"
	req.write_text('Owner = "bob"\nRequestCpus = 1\nRequestMemory = 64\nRequestDisk = 1024\n')
	rv = users["bob"].run(["htcondor", "ocu", "create", req.as_posix()])
	logger.info(f"bob ocu create: rc={rv.returncode} out={rv.stdout} err={rv.stderr}")
	return rv


@action
def unknown_owner_result(condor, alice_known):
	return create_ocu(condor, "nosuchuser")


@action
def the_ocu(condor, alice_known, bob_create_attempt, unknown_owner_result):
	result = create_ocu(condor, "alice", cpus=1)
	assert result["Result"] == 0, result
	return int(result["OCUId"])


@action
def claimed_with_empty_queue(condor, the_ocu):
	try:
		wait_ocus_state(condor, [the_ocu], "I", timeout=90)
	except AssertionError:
		return {"claimed": False, "ocus": query_ocus(condor)}
	slots = wait_ocu_slots(condor, 1)
	# stay claimed for a while with no jobs at all from alice: several
	# negotiation cycles (2s) and schedd passes (5s)
	time.sleep(12)
	return {"claimed": True, "slot": list(slots.keys())[0],
		"ocus": query_ocus(condor), "slots_after": ocu_slots(condor)}


@action
def after_remove(condor, test_dir, users, path_to_sleep, the_ocu, claimed_with_empty_queue):
	schedd = condor.get_local_schedd()
	result = schedd.remove_ocu(the_ocu)
	logger.info(f"remove_ocu -> {result}")
	# bob wants the whole (2-cpu) machine: only possible once the OCU's
	# cpu has been released
	cluster = users["bob"].submit(sleep_job(path_to_sleep, test_dir, 2,
		log_name="bob_after_remove.log", cpus=2))
	try:
		wait_cluster(condor, cluster, {4}, timeout=120)
	except AssertionError:
		pass
	return {"result": result, "ocus": query_ocus(condor), "ocu_slots": ocu_slots(condor),
		"bob": job_ads(condor, cluster)[0]}


@action
def remove_missing_result(condor, after_remove):
	schedd = condor.get_local_schedd()
	return schedd.remove_ocu(987654)


class TestOCULifecycle:
	def test_non_super_user_cannot_create(self, condor, bob_create_attempt):
		ocus = query_ocus(condor)
		assert not any(ad.get("Owner") == "bob" for ad in ocus.values()), ocus
		out = (bob_create_attempt.stdout + bob_create_attempt.stderr).lower()
		assert bob_create_attempt.returncode != 0 or "denied" in out

	def test_unknown_owner_refused(self, unknown_owner_result):
		assert unknown_owner_result["Result"] != 0, unknown_owner_result

	def test_ocu_claimed_with_owner_queue_empty(self, claimed_with_empty_queue):
		assert claimed_with_empty_queue["claimed"], claimed_with_empty_queue["ocus"]

	def test_ocu_stays_claimed_with_owner_queue_empty(self, claimed_with_empty_queue, the_ocu):
		assert claimed_with_empty_queue["claimed"]
		assert claimed_with_empty_queue["ocus"][the_ocu].get("OCUState") == "I"
		assert claimed_with_empty_queue["slot"] in claimed_with_empty_queue["slots_after"]

	def test_remove_releases_claim(self, after_remove, the_ocu):
		assert after_remove["result"]["Result"] == 0, after_remove["result"]
		assert the_ocu not in after_remove["ocus"]
		assert not after_remove["ocu_slots"], after_remove["ocu_slots"]

	def test_resources_usable_after_remove(self, after_remove):
		ad = after_remove["bob"]
		assert ad["JobStatus"] == 4, ad
		assert ad.get("MachineAttrOCU0") is not True, ad

	def test_remove_missing_ocu_fails(self, remove_missing_result):
		assert remove_missing_result["Result"] != 0, remove_missing_result


@action
def restart_ocu(condor, alice_known):
	result = create_ocu(condor, "alice", cpus=1)
	assert result["Result"] == 0, result
	ocu_id = int(result["OCUId"])
	wait_ocus_state(condor, [ocu_id], "I", timeout=90)
	slots = wait_ocu_slots(condor, 1)
	return {"id": ocu_id, "slot": list(slots.keys())[0]}


def _schedd_up(condor):
	try:
		query_ocus(condor)
		return True
	except Exception:
		return False


@action
def after_restart(condor, test_dir, users, path_to_sleep, restart_ocu):
	monitor = SlotMonitor(condor).start()
	try:
		rv = condor.run_command(["condor_restart", "-schedd"])
		assert rv.returncode == 0, rv.stderr
		time.sleep(2)
		wait_for(lambda: _schedd_up(condor), timeout=120, what="schedd back up")
		t_up = time.time()
		try:
			wait_ocus_state(condor, [restart_ocu["id"]], "I", timeout=120)
			reclaimed_after = time.time() - t_up
		except AssertionError:
			reclaimed_after = None
		ocus = query_ocus(condor)
		slots = ocu_slots(condor)
		# Now give alice a job, so the rest of the checks can proceed
		cluster = users["alice"].submit(sleep_job(path_to_sleep, test_dir, 2,
			log_name="alice_after_restart.log", extra={"+OCUWanted": "true"}))
		try:
			wait_cluster(condor, cluster, {4}, timeout=180)
		except AssertionError:
			pass
		time.sleep(3)
	finally:
		monitor.stop()
	return {"reclaimed_after": reclaimed_after, "ocus": ocus, "slots": slots,
		"job": job_ads(condor, cluster)[0], "monitor": monitor,
		"slots_end": ocu_slots(condor)}


class TestOCUScheddRestart:
	def test_ocu_persists(self, after_restart, restart_ocu):
		assert restart_ocu["id"] in after_restart["ocus"], after_restart["ocus"]

	def test_ocu_reclaimed_without_owner_jobs(self, after_restart, restart_ocu):
		assert after_restart["reclaimed_after"] is not None, (
			f"OCU not re-claimed within 120s of schedd restart: {after_restart['ocus']}")

	def test_owner_job_runs_after_restart(self, after_restart):
		ad = after_restart["job"]
		assert ad["JobStatus"] == 4, ad
		assert ad.get("MachineAttrOCU0") is True, ad

	def test_no_stranded_ocu_claim(self, after_restart):
		assert len(after_restart["slots_end"]) == 1, (
			f"expected exactly one OCU slot after restart: {list(after_restart['slots_end'].keys())}")
		assert after_restart["monitor"].max_ocu_slots() <= 1
