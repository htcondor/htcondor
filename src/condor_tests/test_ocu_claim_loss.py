#!/usr/bin/env pytest

# OCU claim loss: the OCU owner's job is running on the OCU when the startd
# kills the claim (condor_vacate -fast), several times, at different points
# into the job.  Each time, the OCU must notice it lost its claim, get a new
# one, and the owner's job must run on it again.  The different delays aim
# at both orders of (startd RELEASE_CLAIM, shadow exit), which the schedd
# must handle without leaving the OCU pointing at a deleted match.

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
	# Two cpus: one for the OCU, and one spare for its replacement claim.
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


# One round at each delay (0, 1 and 2 seconds into the job).
VACATE_ROUNDS = 3


@action
def claim_loss(condor, test_dir, users, path_to_sleep, alice_known):
	"""Repeatedly kill the claim under alice's running OCU job, at varying
	delays after it starts, so that both orders of (startd RELEASE_CLAIM,
	shadow exit) get exercised."""
	result = create_ocu(condor, "alice", cpus=1)
	assert result["Result"] == 0, result
	ocu_id = int(result["OCUId"])
	wait_ocus_state(condor, [ocu_id], "I", timeout=90)
	cluster = users["alice"].submit(sleep_job(path_to_sleep, test_dir, 3600,
		log_name="alice_claim_loss.log", extra={"+OCUWanted": "true"}))

	rounds = []
	starts = 0
	for i in range(VACATE_ROUNDS):
		def running_again():
			ad = job_ads(condor, cluster)[0]
			if ad["JobStatus"] == 2 and ad.get("NumShadowStarts", 0) > starts:
				return ad
			return None
		try:
			ad = wait_for(running_again, timeout=120, what=f"alice's job running (round {i})")
		except AssertionError:
			rounds.append({"round": i, "ran": False, "job": job_ads(condor, cluster)[0],
				"ocus": query_ocus(condor), "slots": ocu_slots(condor)})
			break
		starts = ad.get("NumShadowStarts", 0)
		slot = ad["RemoteHost"]
		time.sleep(i % 3)   # 0, 1, 2 seconds into the job
		# A brand-new dslot may not be in the collector yet; retry until
		# condor_vacate can find it.
		deadline = time.time() + 60
		while True:
			rv = condor.run_command(["condor_vacate", "-fast", slot])
			logger.info(f"round {i}: vacate {slot}: {rv.returncode} {rv.stdout} {rv.stderr}")
			if rv.returncode == 0 or time.time() > deadline:
				break
			time.sleep(1)
		assert rv.returncode == 0, f"could not vacate {slot}"
		rounds.append({"round": i, "ran": True, "slot": slot,
			"ocu_attr": ad.get("MachineAttrOCU0")})

	# and once more: it must come back after the last vacate
	try:
		wait_for(lambda: (lambda a: a["JobStatus"] == 2 and a.get("NumShadowStarts", 0) > starts)(job_ads(condor, cluster)[0]),
			timeout=120, what="alice's job running after the last vacate")
		final_ran = True
	except AssertionError:
		final_ran = False
	time.sleep(2)
	return {"id": ocu_id, "rounds": rounds, "final_ran": final_ran,
		"ocus": query_ocus(condor), "job": job_ads(condor, cluster)[0], "slots": ocu_slots(condor)}


class TestOCUClaimLoss:
	def test_owner_job_reruns_on_ocu_every_time(self, claim_loss):
		stuck = [r for r in claim_loss["rounds"] if not r["ran"]]
		assert not stuck, f"OCU never came back: {stuck}"
		assert claim_loss["final_ran"], f"job never ran after last vacate: {claim_loss['job']} {claim_loss['ocus']}"
		assert all(r["ocu_attr"] is True for r in claim_loss["rounds"]), claim_loss["rounds"]

	def test_ocu_state_running(self, claim_loss):
		assert claim_loss["ocus"][claim_loss["id"]].get("OCUState") == "R", claim_loss["ocus"]

	def test_one_ocu_slot(self, claim_loss):
		assert len(claim_loss["slots"]) == 1, list(claim_loss["slots"].keys())
