#!/usr/bin/env pytest

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

from ornithology import (
    Condor,
    action,
    ClusterState,
)

import time
import htcondor2
import classad2


@action
def the_lock_file(test_dir):
	return test_dir / 'job.started'


@action
def the_condor(test_dir):
	local_dir = test_dir / "condor"
	with Condor(
		local_dir=local_dir,
		config={
			'STARTD_DEBUG':					'D_CATEGORY D_SUBSECOND D_TEST',
			'STARTER_DEBUG':				'D_CATEGORY D_SUBSECOND D_TEST',
			'TESTING_COLOR_FROM_JOB_AD':	'TRUE',
			# By making this explicit, I don't have to look it up later.
			'UPDATE_INTERVAL':				'2',
		},
	) as the_condor:
		yield the_condor


@action
def the_running_job(test_dir, the_lock_file, the_condor):
	job_description = {
		'shell':						f'touch {the_lock_file.as_posix()}; sleep 300',
		'should_transfer_files':		'YES',
		'log':							test_dir / 'job.log',
		'MY.ColorAd':					'[ ColorAttr = 7 ]',
	}

	job_handle = the_condor.submit(
		description=job_description,
		count=1,
	);

	# Wait for the job to be marked running...
	assert job_handle.wait(
		timeout=20,
		condition=ClusterState.all_running,
	)

	# ... wait for the job to have actually started.
	for i in range(0,20):
		if the_lock_file.exists():
			break
		time.sleep(1)
	assert the_lock_file.exists()

	return job_handle


@action
def the_slot_ads(the_condor, the_running_job):
	# Sleep for the UPDATE_INTERVAL + 1 to make sure the startd has updated
	# the slot ads in the collector.
	time.sleep(3)

	result = the_condor.status(
		ad_type=htcondor2.AdType.Slot,
	)
	assert(len(result) > 0)

	return result


@action
def the_extra_ads(the_condor, the_running_job):
	result = the_condor.run_command(
		args=['condor_who', '-l', '-snapshot'],
	)
	assert result.returncode == 0

	ads = list(classad2.parseAds(result.stdout, classad2.ParserType.Old))
	assert(len(ads) > 0)

	ads = [ad for ad in ads if ad.get('MyType', "") == "Machine.Extra"]
	assert(len(ads) > 0)

	return ads


@action
def the_cleaned_up_ads(
	the_condor, the_running_job,
	# Make sure that we've recorded these before we perturb the state.
	the_extra_ads, the_slot_ads
):
	result_ad = the_running_job.remove()
	# assert something about the result_ad here

	# See previous comment about this constant.
	time.sleep(3)

	result = the_condor.status(
		ad_type=htcondor2.AdType.Slot,
	)
	assert(len(result) > 0)

	return result


class TestSlotColoring:

	# This is an implementation details, but may be useful for debugging.
	def test_extra_ads(self, the_extra_ads):
		for extra_ad in the_extra_ads:
			assert extra_ad.get('ColorAttr', 0) == 7


	def test_slot_ads(self, the_slot_ads):
		for slot_ad in the_slot_ads:
			assert slot_ad.get('ColorAttr', 0) == 7


	def test_color_cleanup(self, the_cleaned_up_ads):
		for slot_ad in the_cleaned_up_ads:
			assert slot_ad.get('ColorAttr', -1) == -1
