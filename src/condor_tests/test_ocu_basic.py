#!/usr/bin/env pytest

# Test that OCU (Owner Capacity Unit) creation and job matching works:
# 1. Stand up a personal condor with default partitionable slots
# 2. Submit an OCUWanted job (this creates the SubmitterData the schedd
#    needs before it will negotiate for OCU claims)
# 3. Create an OCU via the Python API
# 4. Wait for the OCU-wanting job to start running on the OCU slot
# 5. While the OCU job is still running (so the OCU slot persists),
#    submit a non-OCU job and verify it does NOT run on the OCU slot

import logging
import time
import getpass

import htcondor2 as htcondor
import classad2 as classad

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@standup
def condor(test_dir):
	with Condor(
		test_dir / "condor",
		config={
			"NUM_CPUS": "4",
			"MEMORY": "4096",
			"DISK": "4096000",
			"NEGOTIATOR_CYCLE_DELAY": "2",
			"NEGOTIATOR_INTERVAL": "2",
		},
	) as condor:
		yield condor


@action
def ocu_job_handle(condor, test_dir, path_to_sleep):
	"""Submit a job that wants an OCU slot.  This must happen before
	the OCU is created so the schedd has a SubmitterData record for
	our user, which is required for the OCU to be negotiated.
	The job sleeps long enough to remain running while we submit
	and verify the non-OCU job."""
	handle = condor.submit(
		description={
			"executable": path_to_sleep,
			"arguments": "300",
			"log": (test_dir / "ocu_job.log").as_posix(),
			"leave_in_queue": True,
			"should_transfer_files": False,
			"Request_Cpus": 1,
			"Request_Memory": 1,
			"Request_Disk": 1,
			"+OCUWanted": "true",
		},
		count=1,
	)

	yield handle
	handle.remove()


@action
def the_ocu(condor, ocu_job_handle):
	"""Create an OCU after the job is submitted so the SubmitterData exists."""
	schedd = condor.get_local_schedd()

	request = classad.ClassAd()
	request["Owner"] = getpass.getuser()
	request["RequestCpus"] = 1
	request["RequestMemory"] = 1
	request["RequestDisk"] = 1

	result = schedd.create_ocu(request)
	logger.debug(f"OCU create result: {result}")
	assert result["Result"] == 0, f"Failed to create OCU: {result.get('ErrorString', 'unknown')}"

	yield result

	# Cleanup: remove the OCU
	try:
		schedd.remove_ocu(int(result["OCUId"]))
	except Exception:
		pass


@action
def ocu_slot_name(condor, ocu_job_handle, the_ocu):
	"""Wait for the OCU job to start running, then extract the slot name.
	We keep the OCU job running so the OCU slot persists while we test
	that a non-OCU job avoids it."""
	ocu_job_handle.wait(
		condition=ClusterState.all_running,
		fail_condition=ClusterState.any_held,
		verbose=True,
		timeout=120,
	)

	for ad in ocu_job_handle.query():
		return get_remote_host(ad)


@action
def non_ocu_job_handle(condor, test_dir, path_to_sleep, ocu_slot_name):
	"""Submit a normal job (no OCUWanted) and wait for completion."""
	handle = condor.submit(
		description={
			"executable": path_to_sleep,
			"arguments": "0",
			"log": (test_dir / "non_ocu_job.log").as_posix(),
			"leave_in_queue": True,
			"should_transfer_files": False,
			"Request_Cpus": 1,
			"Request_Memory": 1,
			"Request_Disk": 1,
		},
		count=1,
	)

	handle.wait(
		condition=ClusterState.all_complete,
		fail_condition=ClusterState.any_held,
		verbose=True,
		timeout=120,
	)

	yield handle
	handle.remove()


@action
def non_ocu_job_ad(non_ocu_job_handle):
	"""Get the completed job ad for the non-OCU job."""
	for ad in non_ocu_job_handle.query():
		return ad


def get_remote_host(job_ad):
	"""Extract the host the job ran on, handling the race between
	RemoteHost and LastRemoteHost."""
	if "LastRemoteHost" in job_ad:
		return job_ad["LastRemoteHost"]
	return job_ad["RemoteHost"]


class TestOCUBasic:
	def test_ocu_created(self, the_ocu):
		"""Verify the OCU was created successfully."""
		assert the_ocu["Result"] == 0
		assert "OCUId" in the_ocu
		assert "OCUName" in the_ocu

	def test_ocu_job_ran_on_ocu_slot(self, ocu_slot_name):
		"""Verify a job with OCUWanted=true was matched to a slot."""
		assert ocu_slot_name is not None

	def test_non_ocu_job_avoided_ocu_slot(self, non_ocu_job_ad, ocu_slot_name):
		"""Verify a normal job did NOT run on the OCU slot."""
		host = get_remote_host(non_ocu_job_ad)
		assert host != ocu_slot_name, (
			f"Non-OCU job ran on OCU slot {ocu_slot_name}, but should not have"
		)
