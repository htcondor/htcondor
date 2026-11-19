#!/usr/bin/env pytest

# OCU borrowing: another user's OCUWilling job may run on an idle OCU, but
# must give it back when the OCU's owner wants it.  Ordinary (not willing)
# jobs must never borrow.  (Preemption policy is covered by test_ocu_isolation;
# this runs with HTCondor's default policy.)
#
#   * alice owns one 1-cpu OCU on a 4-cpu machine, and has a terrible
#     priority factor.
#   * bob fills the 3 non-OCU cpus with long plain jobs, then submits a
#     long OCUWilling job, which can only run by borrowing alice's OCU.
#   * carol submits a long plain job: with no free cpus and the OCU in
#     use or reserved, it must stay idle.
#   * alice submits an OCUWanted job: bob's borrower must be evicted and
#     alice's job must run on her OCU.
#   * after that, bob's borrower borrows again; alice then submits a *held*
#     OCUWanted job, which cannot use the OCU and so must not evict bob.

import logging
import time

import pytest

import htcondor2 as htcondor

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
	return {u: User(condor, test_dir, u) for u in ("alice", "bob", "carol")}


@action
def alice_anchor(condor, test_dir, users, path_to_sleep):
	return users["alice"].submit(sleep_job(path_to_sleep, test_dir, 1,
		log_name="alice_anchor.log", extra={"hold": "True"}))


@action
def the_ocu(condor, alice_anchor):
	set_priority_factor(condor, "alice", 1000000)
	result = create_ocu(condor, "alice")
	assert result["Result"] == 0, result
	ocu_id = int(result["OCUId"])
	wait_ocus_state(condor, [ocu_id], "I")
	return ocu_id


@action
def ocu_slot_name(condor, the_ocu):
	return list(wait_ocu_slots(condor, 1).keys())[0]


@action
def bob_fill(condor, test_dir, users, path_to_sleep, ocu_slot_name):
	cluster = users["bob"].submit(sleep_job(path_to_sleep, test_dir, 3600,
		log_name="bob_fill.log"), count=3)
	wait_cluster(condor, cluster, {2}, timeout=120)
	return cluster


@action
def monitor(condor, ocu_slot_name):
	mon = SlotMonitor(condor).start()
	yield mon
	mon.stop()


@action
def bob_borrower(condor, test_dir, users, path_to_sleep, bob_fill, monitor):
	return users["bob"].submit(sleep_job(path_to_sleep, test_dir, 3600,
		log_name="bob_borrower.log", extra={"+OCUWilling": "true"}))


@action
def carol_plain(condor, test_dir, users, path_to_sleep, bob_fill, monitor):
	return users["carol"].submit(sleep_job(path_to_sleep, test_dir, 3600,
		log_name="carol_plain.log"))


@action
def borrowed(condor, bob_borrower, carol_plain, ocu_slot_name):
	"""Did bob's OCUWilling job borrow alice's idle OCU?"""
	try:
		# JobStatus goes to 2 when the shadow is spawned; RemoteHost and the
		# machine attrs follow once the job is activated on the slot.
		ads = wait_for(lambda: [ad for ad in job_ads(condor, bob_borrower)
				if ad["JobStatus"] == 2 and ad.get("RemoteHost")],
			timeout=90, what="bob's OCUWilling job running")
	except AssertionError:
		return None
	return ads[0]


@action
def alice_reclaim(condor, test_dir, users, path_to_sleep, borrowed, ocu_slot_name):
	"""alice wants her OCU back."""
	cluster = users["alice"].submit(sleep_job(path_to_sleep, test_dir, 5,
		log_name="alice_wanted.log", extra={"+OCUWanted": "true"}))
	try:
		wait_cluster(condor, cluster, {2, 4}, timeout=120)
	except AssertionError:
		pass
	ad_running = job_ads(condor, cluster)[0]
	wait_cluster(condor, cluster, {4}, timeout=180)
	return {"while_running": ad_running, "done": job_ads(condor, cluster)[0],
		"bob": job_ads(condor, borrowed["ClusterId"]) if borrowed else None}


@action
def held_wanted_churn(condor, test_dir, users, path_to_sleep, alice_reclaim, bob_borrower, ocu_slot_name):
	"""After alice's job finishes, a borrower may take the OCU again.  A
	held OCUWanted job of alice's can't use the OCU, so it must not cause
	the borrower to be evicted."""
	try:
		wait_for(lambda: [ad for ad in job_ads(condor, bob_borrower) if ad["JobStatus"] == 2],
			timeout=90, what="bob's OCUWilling job running again")
	except AssertionError:
		return None
	before = job_ads(condor, bob_borrower)[0]
	held = users["alice"].submit(sleep_job(path_to_sleep, test_dir, 5,
		log_name="alice_held_wanted.log", extra={"+OCUWanted": "true", "hold": "True"}))
	# Before the fix, the borrower was evicted within 5s, over and over.
	# Two schedd count_jobs passes (SCHEDD_INTERVAL 5) plus slack.
	time.sleep(12)
	after = job_ads(condor, bob_borrower)[0]
	return {"before": before, "after": after, "held": job_ads(condor, held)[0]}


class TestOCUBorrowing:
	def test_carol_plain_never_on_ocu(self, condor, carol_plain, borrowed, ocu_slot_name):
		ad = job_ads(condor, carol_plain)[0]
		assert ad["JobStatus"] == 1, f"carol's plain job should be idle (no free cpus): {ad}"
		assert ad.get("MachineAttrOCU0") is not True, ad

	def test_bob_fill_never_on_ocu(self, condor, bob_fill, borrowed, ocu_slot_name):
		for ad in job_ads(condor, bob_fill):
			assert ran_on(ad) != ocu_slot_name, ad
			assert ad.get("MachineAttrOCU0") is not True, ad

	def test_willing_job_borrows_idle_ocu(self, borrowed, ocu_slot_name):
		assert borrowed is not None, "bob's OCUWilling job never borrowed alice's idle OCU"
		assert borrowed.get("RemoteHost") == ocu_slot_name, borrowed
		assert borrowed.get("MachineAttrOCU0") is True, borrowed

	def test_owner_reclaims_from_borrower(self, alice_reclaim, ocu_slot_name):
		ad = alice_reclaim["done"]
		assert ad["JobStatus"] == 4, ad
		assert ad.get("MachineAttrOCU0") is True, ad
		assert ran_on(ad) == ocu_slot_name, ad

	def test_borrower_evicted_for_owner(self, test_dir, alice_reclaim, borrowed):
		if borrowed is None:
			pytest.skip("no borrowing happened")
		# Look for the eviction event, not NumJobStarts: the borrower may be
		# evicted while its starter is still setting up, before the job ever
		# executes, and then NumJobStarts stays 0.
		events = htcondor.JobEventLog((test_dir / "bob_borrower.log").as_posix()).events(stop_after=0)
		assert any(e.type == htcondor.JobEventType.JOB_EVICTED for e in events), alice_reclaim["bob"]
		assert alice_reclaim["done"].get("MachineAttrOCU0") is True

	def test_held_ocuwanted_does_not_evict_borrower(self, held_wanted_churn, borrowed):
		if borrowed is None or held_wanted_churn is None:
			pytest.skip("no borrowing happened")
		before = held_wanted_churn["before"]
		after = held_wanted_churn["after"]
		assert after["JobStatus"] == 2, f"borrower was evicted for a held OCUWanted job: {after}"
		# NumShadowStarts, not NumJobStarts: the schedd bumps it when it
		# spawns a shadow, while NumJobStarts lags until the execute event.
		assert after.get("NumShadowStarts") == before.get("NumShadowStarts"), (before, after)

	def test_ocu_slot_only_owner_or_willing_borrower(self, monitor, held_wanted_churn, ocu_slot_name):
		allowed = {"alice", "bob"}
		seen = monitor.ocu_slot_users()
		assert not (seen.get(ocu_slot_name, set()) - allowed), seen
		assert "carol" not in seen.get(ocu_slot_name, set())

	def test_no_extra_ocu_claims(self, monitor, held_wanted_churn):
		assert monitor.max_ocu_slots() <= 1

	def test_ocu_claim_never_lost(self, condor, monitor, held_wanted_churn, the_ocu, ocu_slot_name):
		lost = monitor.lost_ocu_slots([ocu_slot_name])
		assert not lost, f"OCU slot stopped being an OCU slot: {lost[:5]}"
		assert query_ocus(condor)[the_ocu].get("OCUState") in ("I", "R")
