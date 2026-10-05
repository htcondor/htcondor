#!/usr/bin/env pytest

import htcondor2 as htcondor

from ornithology import (
    Condor,
    action,
    ClusterState,
)

import logging
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@action
def the_condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor",
    ) as condor:
        yield condor


TEST_CASES = {
	"min_t": {
		"jdl": { 'min_condor_version': '20' },
		"condition": ClusterState.all_running,
		"fail_condition": ClusterState.any_terminal,
	},
	"max_t": {
		"jdl": { 'max_condor_version': '30' },
		"condition": ClusterState.all_running,
		"fail_condition": ClusterState.any_terminal,
	 },
	"minmax_t": {
		"jdl": {
			'min_condor_version': '20',
			'max_condor_version': '30',
		},
		"condition": ClusterState.all_running,
		"fail_condition": ClusterState.any_terminal,
	},
	"min_f": {
		"jdl": { 'min_condor_version': '30' },
		"condition": ClusterState.all_idle,
		"fail_condition": ClusterState.any_running,
	},
	"max_f":{
		"jdl": { 'max_condor_version': '20' },
		"condition": ClusterState.all_idle,
		"fail_condition": ClusterState.any_running,
	 },
	"minmax_f": {
		"jdl": {
			'min_condor_version': '30',
			'max_condor_version': '40',
		},
		"condition": ClusterState.all_idle,
		"fail_condition": ClusterState.any_running,
	},
}


@action
def the_job_handles(test_dir, the_condor):
	job_description = {
		"shell":	"sleep 60",
		"log":		test_dir / "jobs.log",
	}

	job_handles = {}
	for name, test_case in TEST_CASES.items():
		complete_job_description = {
			** job_description,
			** test_case['jdl'],
		}

		job_handles[name] = the_condor.submit(
			description = complete_job_description,
			count=1
		)

	yield job_handles


class TestMinMaxCondorVersion:

	def test_as_expected(self, the_job_handles):
		# For this to work, the tests that cause wait() to block until
		# their job has started must come first.  (We're assuming that
		# the idle jobs will start at the same time, if they do.)
		for name, job_handle in the_job_handles.items():
			assert job_handle.wait(
				timeout=60,
				condition=TEST_CASES[name]['condition'],
				fail_condition=TEST_CASES[name]['fail_condition'],
			)
