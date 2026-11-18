#!/usr/bin/env pytest

# OCU isolation: jobs that are not the OCU owner's OCUWanted jobs must never
# run on an OCU slot, and the OCU claim must never be taken away from its
# owner -- whether or not the negotiator is allowed to preempt.
#
#   * alice owns two 1-cpu OCUs on a 4-cpu machine.
#   * alice's priority factor is made terrible, so with preemption on,
#     every other user outranks her and the negotiator would happily
#     preempt her (idle!) OCU claims if nothing protects them.
#   * bob floods the pool with plain jobs, alice submits plain jobs,
#     and bob submits an OCUWanted job even though he owns no OCU.
#
# Checked throughout (by sampling the startd) and at the end (from the
# job ads):
#   * no plain job ran on an OCU slot,
#   * bob's OCUWanted job did not run on alice's OCU,
#   * the OCU slots stayed OCU slots, claimed by alice, the whole time,
#   * the schedd still shows both OCUs as claimed (Idle).

import logging
import time

from ornithology import *

from ocu_helpers import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@config(params={k: k for k in PREEMPTION_CONFIGS})
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
	"""A held job, so alice has a user record and a submitter in the
	schedd -- an OCU can only be created for, and negotiated by, a known
	user."""
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
	slots = wait_ocu_slots(condor, 2)
	for name, ad in slots.items():
		logger.info(f"OCU slot {name}: {ad}")
	return sorted(slots.keys())


@action
def scenario(condor, test_dir, users, path_to_sleep, the_ocus, ocu_slot_names):
	monitor = SlotMonitor(condor).start()
	try:
		# 2 cpus are free; 4 bob jobs and 2 alice plain jobs compete for them.
		# Short jobs so there is claim churn and several negotiation cycles.
		bob_plain = users["bob"].submit(sleep_job(path_to_sleep, test_dir, 3,
			log_name="bob_plain.log"), count=4)
		alice_plain = users["alice"].submit(sleep_job(path_to_sleep, test_dir, 3,
			log_name="alice_plain.log"), count=2)
		# bob owns no OCU; his OCUWanted job must not take alice's.
		bob_wanted = users["bob"].submit(sleep_job(path_to_sleep, test_dir, 3,
			log_name="bob_wanted.log", extra={"+OCUWanted": "true"}))

		wait_cluster(condor, bob_plain, {4}, timeout=300)
		wait_cluster(condor, alice_plain, {4}, timeout=300)
		# Give bob's OCUWanted job (and the negotiator, every 2s) a few
		# more cycles with nothing else to match
		time.sleep(8)
	finally:
		monitor.stop()

	return {
		"monitor": monitor,
		"bob_plain": job_ads(condor, bob_plain),
		"alice_plain": job_ads(condor, alice_plain),
		"bob_wanted": job_ads(condor, bob_wanted),
		"ocus": query_ocus(condor),
		"ocu_slots_after": ocu_slots(condor),
	}


class TestOCUIsolation:
	def test_ocus_claimed(self, the_ocus, ocu_slot_names):
		assert len(ocu_slot_names) == 2

	def test_ocu_slot_claimed_by_owner(self, condor, ocu_slot_names):
		slots = ocu_slots(condor)
		for name in ocu_slot_names:
			assert bare(slots[name].get("RemoteOwner")) in (None, "alice"), slots[name]

	def test_plain_jobs_never_on_ocu_slot(self, scenario, ocu_slot_names):
		bad = []
		for key in ("bob_plain", "alice_plain"):
			for ad in scenario[key]:
				if ad.get("MachineAttrOCU0") is True or ran_on(ad) in ocu_slot_names:
					bad.append((key, ad["ClusterId"], ad["ProcId"], ran_on(ad), ad.get("MachineAttrOCUName0")))
		assert not bad, f"plain jobs ran on OCU slots: {bad}"

	def test_non_owner_ocuwanted_not_on_owners_ocu(self, scenario, ocu_slot_names):
		ad = scenario["bob_wanted"][0]
		assert ran_on(ad) not in ocu_slot_names, ad
		assert ad["JobStatus"] == 1, f"bob's OCUWanted job should still be idle: {ad}"

	def test_ocu_slots_only_used_by_owner(self, scenario, ocu_slot_names):
		users_seen = scenario["monitor"].ocu_slot_users()
		bad = {n: u for n, u in users_seen.items() if u - {"alice"}}
		assert not bad, f"OCU slots used by non-owners: {bad}"

	def test_no_extra_ocu_claims(self, scenario, the_ocus):
		assert scenario["monitor"].max_ocu_slots() <= len(the_ocus)

	def test_ocu_claims_never_lost(self, scenario, ocu_slot_names):
		mon = scenario["monitor"]
		assert len(mon.samples) > 10
		lost = mon.lost_ocu_slots(ocu_slot_names)
		assert not lost, f"OCU slots stopped being OCU slots: {lost[:5]}"
		others = [x for x in mon.non_ocu_owners_on(ocu_slot_names) if x[2] != "alice"]
		assert not others, f"OCU slots had non-owner RemoteOwner: {others[:5]}"

	def test_ocus_still_claimed_at_end(self, scenario, the_ocus, ocu_slot_names):
		for i in the_ocus:
			assert scenario["ocus"][i].get("OCUState") == "I", scenario["ocus"][i]
		assert sorted(scenario["ocu_slots_after"].keys()) == ocu_slot_names
