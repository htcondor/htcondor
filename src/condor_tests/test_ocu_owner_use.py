#!/usr/bin/env pytest

# OCU owner use: the OCU owner's OCUWanted jobs run on, and only on, the
# owner's OCU claims; the claims are reused for job after job and are held
# (idle) between jobs; and the owner's jobs keep working while another user
# saturates the rest of the pool.  (Preemption policy is covered by
# test_ocu_isolation; this runs with HTCondor's default policy.)
#
#   * alice owns two 1-cpu OCUs on a 4-cpu machine, and has a terrible
#     priority factor.
#   * bob keeps the 2 non-OCU cpus busy with long jobs.
#   * alice submits 4 short OCUWanted jobs.  At most two can run at once
#     (one per OCU), every one must run on an OCU slot, and all must
#     finish: jobs 3 and 4 can only run by reusing the idle OCU claims.
#   * alice submits a 2-cpu OCUWanted job, which fits none of her 1-cpu
#     OCUs.  It must stay idle (not run on a non-OCU slot, and not
#     disturb the OCU claims).
#   * after everything, both OCUs are back to Idle on the same slots,
#     still claimed by alice.

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
	with Condor(test_dir / "condor", config=base_config(preemption)) as condor:
		yield condor


@action
def users(condor, test_dir):
	return {u: User(condor, test_dir, u) for u in ("alice", "bob")}


@action
def alice_anchor(condor, test_dir, users, path_to_sleep):
	return users["alice"].submit(sleep_job(path_to_sleep, test_dir, 1,
		log_name="alice_anchor.log", extra={"hold": "True"}))


@action
def the_ocus(condor, alice_anchor):
	set_priority_factor(condor, "alice", 1000000)
	ids = []
	for _ in range(2):
		result = create_ocu(condor, "alice")
		assert result["Result"] == 0, result
		ids.append(int(result["OCUId"]))
	wait_ocus_state(condor, ids, "I")
	return ids


@action
def ocu_slot_names(condor, the_ocus):
	return sorted(wait_ocu_slots(condor, 2).keys())


@action
def bob_busy(condor, test_dir, users, path_to_sleep, ocu_slot_names):
	"""bob fills the non-OCU cpus with long jobs (and has more waiting)"""
	cluster = users["bob"].submit(sleep_job(path_to_sleep, test_dir, 600,
		log_name="bob_busy.log"), count=4)
	wait_for(lambda: sum(1 for ad in job_ads(condor, cluster) if ad["JobStatus"] == 2) >= 2,
		timeout=120, what="2 of bob's jobs running")
	return cluster


@action
def scenario(condor, test_dir, users, path_to_sleep, the_ocus, ocu_slot_names, bob_busy):
	monitor = SlotMonitor(condor).start()
	max_running = 0
	ocu_states_seen = set()
	try:
		alice_wanted = users["alice"].submit(sleep_job(path_to_sleep, test_dir, 3,
			log_name="alice_wanted.log", extra={"+OCUWanted": "true"}), count=4)

		deadline = time.time() + 300
		while time.time() < deadline:
			ads = job_ads(condor, alice_wanted)
			running = sum(1 for ad in ads if ad["JobStatus"] == 2)
			max_running = max(max_running, running)
			for ad in query_ocus(condor).values():
				ocu_states_seen.add(ad.get("OCUState"))
			if ads and all(ad["JobStatus"] == 4 for ad in ads):
				break
			time.sleep(1)

		# A job that fits none of alice's OCUs
		alice_too_big = users["alice"].submit(sleep_job(path_to_sleep, test_dir, 5,
			log_name="alice_too_big.log", cpus=2, extra={"+OCUWanted": "true"}))
		# several negotiation cycles (every 2s) and schedd passes (every 5s)
		time.sleep(8)

		# The claims must still be good: run one more OCUWanted job on them.
		alice_again = users["alice"].submit(sleep_job(path_to_sleep, test_dir, 1,
			log_name="alice_again.log", extra={"+OCUWanted": "true"}))
		try:
			wait_cluster(condor, alice_again, {4}, timeout=120)
		except AssertionError as e:
			logger.info(f"alice_again did not finish: {e}")
		time.sleep(3)
	finally:
		monitor.stop()

	return {
		"monitor": monitor,
		"max_running": max_running,
		"ocu_states_seen": ocu_states_seen,
		"alice_wanted": job_ads(condor, alice_wanted),
		"alice_too_big": job_ads(condor, alice_too_big),
		"alice_again": job_ads(condor, alice_again),
		"bob_busy": job_ads(condor, bob_busy),
		"ocus": query_ocus(condor),
		"ocu_slots_after": ocu_slots(condor),
	}


class TestOCUOwnerUse:
	def test_all_owner_jobs_completed(self, scenario):
		ads = scenario["alice_wanted"]
		assert len(ads) == 4
		assert all(ad["JobStatus"] == 4 for ad in ads), ads

	def test_owner_job_runs_while_others_busy(self, scenario):
		# bob still has long jobs queued; alice's OCU must still be hers
		ad = scenario["alice_again"][0]
		assert ad["JobStatus"] == 4, ad

	def test_owner_jobs_ran_on_owner_ocus(self, scenario, the_ocus, ocu_slot_names):
		for ad in scenario["alice_wanted"] + scenario["alice_again"]:
			assert ad.get("MachineAttrOCU0") is True, ad
			assert ad.get("MachineAttrOCUId0") in the_ocus, ad
			assert ran_on(ad) in ocu_slot_names, ad

	def test_at_most_one_job_per_ocu(self, scenario):
		assert 1 <= scenario["max_running"] <= 2

	def test_ocu_state_tracks_running(self, scenario):
		assert "R" in scenario["ocu_states_seen"]

	def test_each_owner_job_started_once(self, scenario):
		# No eviction/restart churn of the owner's jobs
		for ad in scenario["alice_wanted"]:
			assert ad.get("NumJobStarts", 0) == 1, ad

	def test_too_big_job_stays_idle(self, scenario, ocu_slot_names):
		ad = scenario["alice_too_big"][0]
		assert ad["JobStatus"] == 1, ad
		assert ad.get("NumShadowStarts", 0) == 0, ad

	def test_bob_never_on_ocu(self, scenario, ocu_slot_names):
		for ad in scenario["bob_busy"]:
			assert ad.get("MachineAttrOCU0") is not True, ad
			assert ran_on(ad) not in ocu_slot_names, ad
		users_seen = scenario["monitor"].ocu_slot_users()
		bad = {n: u for n, u in users_seen.items() if u - {"alice"}}
		assert not bad, f"OCU slots used by non-owners: {bad}"

	def test_bob_jobs_not_preempted(self, scenario):
		# bob's jobs ran on ordinary slots and nothing about alice's OCU
		# use should have evicted them
		for ad in scenario["bob_busy"]:
			assert ad.get("NumJobStarts", 0) <= 1, ad

	def test_no_extra_ocu_claims(self, scenario, the_ocus):
		assert scenario["monitor"].max_ocu_slots() <= len(the_ocus)

	def test_ocu_claims_never_lost(self, scenario, ocu_slot_names):
		lost = scenario["monitor"].lost_ocu_slots(ocu_slot_names)
		assert not lost, f"OCU slots stopped being OCU slots: {lost[:5]}"

	def test_ocus_idle_and_held_at_end(self, scenario, the_ocus, ocu_slot_names):
		for i in the_ocus:
			assert scenario["ocus"][i].get("OCUState") == "I", scenario["ocus"][i]
		assert sorted(scenario["ocu_slots_after"].keys()) == ocu_slot_names

	def test_owner_activations_counted(self, scenario, the_ocus):
		total = sum(int(scenario["ocus"][i].get("OCUOwnerActivations", 0)) for i in the_ocus)
		assert total == 5, {i: dict(scenario["ocus"][i]) for i in the_ocus}
