#!/usr/bin/env pytest

#testreq: personal
"""<<CONDOR_TESTREQ_CONFIG
	# make sure that file transfer plugins are enabled (might be disabled by default)
	ENABLE_URL_TRANSFERS = true
"""
#endtestreq

import logging
import os

from ornithology import *
from pathlib import Path

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@action
def tor_job(default_condor, path_to_sleep, test_dir):
    return default_condor.submit(
        {
            "executable":               path_to_sleep,
            "transfer_executable":      "False",
            "should_transfer_files":    "True",

            "log":                      test_dir / "tor_job.log",
            "hold":                     "True",

            "output":                   "output",
            "transfer_output_remaps":   '"output=badname://foo/bar"',
        },
        count=1,
    )


@action
def tor_job_requirements(tor_job):
    return str(tor_job.clusterad["Requirements"])


@action
def nph_job(default_condor, path_to_sleep, test_dir):
    # The idea here is that the job will fail the first time because the
    # output remap points to a bad URL. This used to cause the job to be held, but
    # with the fix in HTCONDOR-2952 the job should just go back to idle in the event 
    # that an EP failed to launch a transfer plugin. The second time the job runs, it should 
    # succeed because the remap expression is designed to only point to the bad URL on the first attempt.
    handle = default_condor.submit(
        {
            "executable":               path_to_sleep,
            "transfer_executable":      "False",
            "should_transfer_files":    "True",
            "arguments":                1,

            "log":                      test_dir / "nph_job.log",

            "transfer_output_files":    path_to_sleep,
            "+TransferOutputRemaps":   f'NumJobStarts > 0 ? "" :  "{path_to_sleep}=badname://foo/bar"',
            "requirements":             'TARGET.HasFileTransferPluginMethods =!= undefined',
        },
        count=1,
    )
    return handle, default_condor.get_local_schedd()

@action
def nph_input_job(default_condor, path_to_sleep, test_dir):
    # The idea here is that the job will fail the first time because the
    # input points to a bad URL. This used to cause the job to be held, but
    # with the fix in HTCONDOR-2952 the job should just go back to idle in the event 
    # that an EP failed to launch a transfer plugin. The second time the job runs, it should 
    # succeed because the TransferInput expression is designed to only point to the bad URL on the first attempt.
    handle = default_condor.submit(
        {
            "executable":               path_to_sleep,
            "transfer_executable":      "False",
            "should_transfer_files":    "True",
            "arguments":                1,

            "log":                      test_dir / "nph_input_job.log",

            "+TransferInput":   f'NumShadowStarts > 1 ? "" :  "badname://foo/bar"',
            "requirements":             'TARGET.HasFileTransferPluginMethods =!= undefined',
        },
        count=1,
    )
    return handle, default_condor.get_local_schedd()

@action
def missing_result_input(default_condor, path_to_sleep, test_dir):
    # The idea here is that the job will fail the first time because the
    # plugin will not return an result file. This used to cause the job to be held, but
    # with the fix in HTCONDOR-2952 the job should just go back to idle in the event 
    # that an EP failed to launch a transfer plugin. The second time the job runs, it should 
    # succeed because the TransferInput expression is designed to only point to the bad URL on the first attempt.
    handle = default_condor.submit(
        {
            "executable":               path_to_sleep,
            "transfer_executable":      "False",
            "should_transfer_files":    "True",
            "arguments":                1,

            "log":                      test_dir / "missing_result_input.log",

            "+TransferInput":   f'NumShadowStarts > 1 ? "" :  "debug://exitnoad/0"',
        },
        count=1,
    )
    return handle, default_condor.get_local_schedd()

'''
This test is commented out for now as it results in the shadow/starter disconnecting multiple times... will fix at
a later date.
@action
def missing_result_output(default_condor, path_to_sleep, test_dir):
    # The idea here is that the job will fail the first time because the
    # plugin will not return an result file. This used to cause the job to be held, but
    # with the fix in HTCONDOR-2952 the job should just go back to idle in the event 
    # that an EP failed to launch a transfer plugin. The second time the job runs, it should 
    # succeed because the TransferInput expression is designed to only point to the bad URL on the first attempt.
    handle = default_condor.submit(
        {
            "executable":               path_to_sleep,
            "transfer_executable":      "False",
            "should_transfer_files":    "True",
            "arguments":                1,

            "log":                      test_dir / "missing_result_output.log",

            "transfer_output_files":    path_to_sleep,
            "+TransferOutputRemaps":   f'NumJobStarts > 0 ? "" :  "{path_to_sleep}=debug://exitnoad/0"',
        },
        count=1,
    )
    return handle, default_condor.get_local_schedd()
'''

@action
def bad_result_input(default_condor, path_to_sleep, test_dir):
    # The idea here is that the job will fail the first time because the
    # plugin will not return a valid result file. This used to cause the job to be held, but
    # with the fix in HTCONDOR-2952 the job should just go back to idle in the event 
    # that an EP failed to launch a transfer plugin. The second time the job runs, it should 
    # succeed because the TransferInput expression is designed to only point to the bad URL on the first attempt.
    handle = default_condor.submit(
        {
            "executable":               path_to_sleep,
            "transfer_executable":      "False",
            "should_transfer_files":    "True",
            "arguments":                1,

            "log":                      test_dir / "bad_result_input.log",

            "+TransferInput":   f'NumShadowStarts > 1 ? "" :  "debug://exitbadad/0"',
        },
        count=1,
    )
    return handle, default_condor.get_local_schedd()

@action
def trailing_semi_job(default_condor, test_dir):
    os.mkdir("sandbox")
    handle = default_condor.submit(
        {
            "executable":               "/usr/bin/touch",
            "transfer_executable":      "False",
            "should_transfer_files":    "True",
            "arguments":                "one two three",

            "log":                      test_dir / "trailing_semi_job.log",

            "transfer_output_remaps":   '"one=sandbox/job_one;two=sandbox/job_two;three=sandbox/job_three;"',
            "output":                   "job_stdout",
            "error":                    "job_stderr",
        },
        count=1,
    )
    return handle


class TestFTPluginStuff:

    def test_tor_requirements(self, tor_job_requirements):
        # How specific do we have to get here?
        assert 'badname' in tor_job_requirements

    def test_no_remap_plugin_vacate(self, nph_job):
        nph_job[0].wait(
            condition=ClusterState.all_complete,
            fail_condition=ClusterState.any_held,
            timeout=180,
        )
        # Verify the job did not go on hold, retried output transfer after plugin failure, and completed successfully.
        assert nph_job[0].state.all_complete()
        # Verify the job went through a vacate cycle with the expected subcode for a missing plugin (-1001)
        assert nph_job[1].jobEpochHistory(
            constraint=f'ClusterID == {nph_job[0].clusterid} && ProcID == 0',
            projection=["VacateReasonSubCode"],
        )[1]["VacateReasonSubCode"] == -1001

    def test_no_input_plugin_vacate(self, nph_input_job):
        nph_input_job[0].wait(
            condition=ClusterState.all_complete,
            fail_condition=ClusterState.any_held,
            timeout=180,
        )
        # Verify the job did not go on hold, retried input transfer after plugin failure, and completed successfully.
        assert nph_input_job[0].state.all_complete()
        # Verify the job went through a vacate cycle with the expected subcode for a missing plugin (-1001)
        assert nph_input_job[1].jobEpochHistory(
            constraint=f'ClusterID == {nph_input_job[0].clusterid} && ProcID == 0',
            projection=["VacateReasonSubCode"],
        )[1]["VacateReasonSubCode"] == -1001

    def test_input_no_plugin_results_vacate(self, missing_result_input):
        missing_result_input[0].wait(
            condition=ClusterState.all_complete,
            fail_condition=ClusterState.any_held,
            timeout=180,
        )
        # Verify the job did not go on hold, retried input transfer after plugin failure, and completed successfully.
        assert missing_result_input[0].state.all_complete()
        # Verify the job went through a vacate cycle with the expected subcode for a missing plugin results file (-1004)
        assert missing_result_input[1].jobEpochHistory(
            constraint=f'ClusterID == {missing_result_input[0].clusterid} && ProcID == 0',
            projection=["VacateReasonSubCode"],
        )[1]["VacateReasonSubCode"] == -1004

    '''
    This test is commented out for now as it results in the shadow/starter disconnecting multiple times... will fix at
    a later date.
    def test_output_no_plugin_results_vacate(self, missing_result_output):
        missing_result_output[0].wait(
            condition=ClusterState.all_complete,
            fail_condition=ClusterState.any_held,
            timeout=60,
        )
        # Verify the job did not go on hold, retried output transfer after plugin failure, and completed successfully.
        assert missing_result_output[0].state.all_complete()
        # Verify the job went through a vacate cycle with the expected subcode for a missing plugin results file (-1004)
        assert missing_result_output[1].jobEpochHistory(
            constraint=f'ClusterID == {missing_result_output[0].clusterid} && ProcID == 0',
            projection=["VacateReasonSubCode"],
        )[1]["VacateReasonSubCode"] == -1004
    '''

    def test_input_bad_plugin_results_vacate(self, bad_result_input):
        bad_result_input[0].wait(
            condition=ClusterState.all_complete,
            fail_condition=ClusterState.any_held,
            timeout=180,
        )
        # Verify the job did not go on hold, retried input transfer after plugin failure, and completed successfully.
        assert bad_result_input[0].state.all_complete()
        # Verify the job went through a vacate cycle with the expected subcode for a missing plugin results file (-1004)
        assert bad_result_input[1].jobEpochHistory(
            constraint=f'ClusterID == {bad_result_input[0].clusterid} && ProcID == 0',
            projection=["VacateReasonSubCode"],
        )[1]["VacateReasonSubCode"] == -1004

    def test_trailing_semi_remap(self, trailing_semi_job):
        trailing_semi_job.wait(
            condition=ClusterState.any_complete,
            fail_condition=ClusterState.all_held,
            timeout=60,
        )
        assert os.path.isfile("job_stdout")
        assert os.path.isfile("job_stderr")
        assert os.path.isfile("sandbox/job_one")
        assert os.path.isfile("sandbox/job_two")
        assert os.path.isfile("sandbox/job_three")
