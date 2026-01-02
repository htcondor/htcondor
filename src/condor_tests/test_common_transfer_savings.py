#!/usr/bin/env pytest

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

from ornithology import (
    Condor,
    action,
    ClusterState,
)

from pathlib import Path

import htcondor2


@action
def the_container_image(test_dir, pytestconfig):
    # This is a gross hack.
    ctest_path = test_dir / ".." / "busybox.sif"
    if ctest_path.exists():
        return ctest_path
    else:
        return Path(pytestconfig.rootdir) / "busybox.sif"


#
# This script runs two (sets of) tests against the same set of jobs, which
# sweep the following parameters:
#
# * the number of common transfers (0, 1, many = 2), including
#   1 - `MY.CommonInputFiles`
#   2 - `_x_common_input_catalogs`
#   3 - `container_image`, with the appropriate config knobs set;
# * the protocol(s) being used (CEDAR, other, or both); and
# * the number of fall-back transfers (0, 1, many = 2).
#
# The first set of tests assert that the record(s) in the epoch log
# match what (should have) happened.
#
# The second set of tests assert that the analysis tool produces the
# correct results (and intermediate results, if appropriate).
#


#
# When the number of common transfers is 0, we don't actually care about the
# number of protocols, and the number of fall-back transfers must be 0.
#

# In these TEST_CASES, an 'expected ad' is the subset of attributes which
# must be present (or, if None, must NOT be present), and not the whole ad.

TEST_CASES = {
    "zero_cxfers": {
        'submit_commands':      {},
        'expected_common_ads':  [],
        'expected_epoch_ads':   [
            {},
            {},
            {},
            {},
            {},
            {},
            {},
            {},
        ],
        'expected_cts_output': 'Found no common file transfer for cluster ID {clusterID}.',
        'fall_back_transfers':  0,
    },

    "one_cxfer_cedar_1": {
        'submit_commands':      {
            'MY.CommonInputFiles':                  '"{path_to_common_input}"',
        },
        'expected_common_ads':   [
            {'CedarSizeBytes'},
            {'CedarSizeBytes'},
        ],
        'expected_epoch_ads':  [
            {'CommonFilesMappedTime', 'CommonInputFiles'},
            {'CommonFilesMappedTime', 'CommonInputFiles'},
            {'CommonFilesMappedTime', 'CommonInputFiles'},
            {'CommonFilesMappedTime', 'CommonInputFiles'},
            {'CommonFilesMappedTime', 'CommonInputFiles'},
            {'CommonFilesMappedTime', 'CommonInputFiles'},
            {'CommonFilesMappedTime', 'CommonInputFiles'},
            {'CommonFilesMappedTime', 'CommonInputFiles'},
        ],
        'expected_cts_output': 'Cluster {clusterID}: required 168 total bytes in common files (as 21 bytes per epoch * 8 epochs, not including fall-back epochs), but only transferred 42 bytes, skipping 126 bytes, or 75% of the total.',
        'fall_back_transfers':  0,
    },

    "one_cxfer_cedar_2": {
        'submit_commands':      {
            'MY._x_common_input_catalogs':      '"A"',
            'MY._x_catalog_A':                  '"{path_to_common_input}"',
        },
        'expected_common_ads':   [
            {'CedarSizeBytes'},
            {'CedarSizeBytes'},
        ],
        'expected_epoch_ads':  [
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_A'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_A'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_A'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_A'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_A'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_A'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_A'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_A'},
        ],
        'expected_cts_output': 'Cluster {clusterID}: required 168 total bytes in common files (as 21 bytes per epoch * 8 epochs, not including fall-back epochs), but only transferred 42 bytes, skipping 126 bytes, or 75% of the total.',
        'fall_back_transfers':  0,
    },

    "one_cxfer_cedar_3": {
        'submit_commands':      {
            'transfer_input_files':     '{path_to_common_input}',
            'container_image':          '{the_container_image}',
            'container_is_common':      'True',
        },
        'expected_common_ads':   [
            {'CedarSizeBytes'},
            {'CedarSizeBytes'},
        ],
        'expected_epoch_ads':  [
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_condor_container_image'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_condor_container_image'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_condor_container_image'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_condor_container_image'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_condor_container_image'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_condor_container_image'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_condor_container_image'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_condor_container_image'},
        ],
        'expected_cts_output': 'Cluster {clusterID}: required 6356992 total bytes in common files (as 794624 bytes per epoch * 8 epochs, not including fall-back epochs), but only transferred 1589248 bytes, skipping 4767744 bytes, or 75% of the total.',
        'fall_back_transfers':  0,
    },

    # We don't want use to debug:// because we'll want the container image.
    "one_cxfer_file_1": {
        'submit_commands':      {
            'MY.CommonInputFiles':                  '"file://{path_to_common_input}"',
        },
        'expected_common_ads':   [
            {'CommonPluginResultList'},
            {'CommonPluginResultList'},
        ],
        'expected_epoch_ads':  [
            {'CommonFilesMappedTime', 'CommonInputFiles'},
            {'CommonFilesMappedTime', 'CommonInputFiles'},
            {'CommonFilesMappedTime', 'CommonInputFiles'},
            {'CommonFilesMappedTime', 'CommonInputFiles'},
            {'CommonFilesMappedTime', 'CommonInputFiles'},
            {'CommonFilesMappedTime', 'CommonInputFiles'},
            {'CommonFilesMappedTime', 'CommonInputFiles'},
            {'CommonFilesMappedTime', 'CommonInputFiles'},
        ],
        'expected_cts_output': 'Cluster {clusterID}: required 168 total bytes in common files (as 21 bytes per epoch * 8 epochs, not including fall-back epochs), but only transferred 42 bytes, skipping 126 bytes, or 75% of the total.',
        'fall_back_transfers':  0,
    },

    "one_cxfer_file_2": {
        'submit_commands':      {
            'MY._x_common_input_catalogs':      '"A"',
            'MY._x_catalog_A':                  '"file://{path_to_common_input}"',
        },
        'expected_common_ads':   [
            {'CommonPluginResultList'},
            {'CommonPluginResultList'},
        ],
        'expected_epoch_ads':  [
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_A'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_A'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_A'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_A'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_A'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_A'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_A'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_A'},
        ],
        'expected_cts_output': 'Cluster {clusterID}: required 168 total bytes in common files (as 21 bytes per epoch * 8 epochs, not including fall-back epochs), but only transferred 42 bytes, skipping 126 bytes, or 75% of the total.',
        'fall_back_transfers':  0,
    },

    "one_cxfer_file_3": {
        'submit_commands':      {
            'transfer_input_files':     '{path_to_common_input}',
            'container_image':          'file://{the_container_image}',
            'container_is_common':      'True',
        },
        'expected_common_ads':   [
            {'CommonPluginResultList'},
            {'CommonPluginResultList'},
        ],
        'expected_epoch_ads':  [
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_condor_container_image'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_condor_container_image'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_condor_container_image'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_condor_container_image'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_condor_container_image'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_condor_container_image'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_condor_container_image'},
            {'CommonFilesMappedTime', '_x_common_input_catalogs', '_x_catalog_condor_container_image'},
        ],
        'expected_cts_output': 'Cluster {clusterID}: required 6356992 total bytes in common files (as 794624 bytes per epoch * 8 epochs, not including fall-back epochs), but only transferred 1589248 bytes, skipping 4767744 bytes, or 75% of the total.',
        'fall_back_transfers':  0,
    },
}

    
SKIPPED_TEST_CASES = {
    "one_cxfer_both_1": {
        'submit_commands':      {},
        'expected_common_ads':  [],
        'expected_epoch_ads':   [],
        'fall_back_transfers':  0,
    },
    "one_cxfer_both_2_single": {
        'submit_commands':      {},
        'expected_common_ads':  [],
        'expected_epoch_ads':   [],
        'fall_back_transfers':  0,
    },
    "one_cxfer_both_2_double": {
        'submit_commands':      {},
        'expected_common_ads':  [],
        'expected_epoch_ads':   [],
        'fall_back_transfers':  0,
    },
    # one_cxfer_both_3 is impossible, since you can't have >1 container image.

    # We'll skip to the most complicated test case; we can fill in the other
    # cases if this one fails but none of the other ones do.
    "three_cxfers_2_double": {
        'submit_commands':      {},
        'expected_common_ads':  [],
        'expected_epoch_ads':   [],
        'fall_back_transfers':  0,
    },
}

# Assuming the number of fall-back transfers will be controlled by HTCondor
# configuration (which may be its own tricky problem), we'll simplify things
# here and assume than we can sweep that parameter serially.


@action
def path_to_common_input(test_dir):
    path = test_dir / 'common-input.file'
    path.write_text('the common input file')
    return path


@action
def the_job_description(test_dir):
    for i in range(0,8):
        Path(f'input-file-{i}').write_text(str(i))

    return {
        'universe':                 "vanilla",
        'shell':
            "ls -la > output-file-$(ProcID); "
            "cat common-file input-file-$(ProcID) >> output-file-$(ProcID); "
            "sleep 5",
        'transfer_input_files':     'input-file-$(ProcID)',
        'transfer_output_files':    'output-file-$(ProcID)',
        'should_transfer_files':    'YES',
        'request_cpus':             1,
        'request_memory':           1024,
        'log':
            f'{(test_dir / "log").as_posix()}.$(ClusterID)',
    }


@action
def the_condor(test_dir):
    local_dir = test_dir / "condor"

    with Condor(
        local_dir=local_dir,
        config={
            'NUM_CPUS':                         4,
            'STARTER_TEST_SANDBOX_TIMEOUT':     8,
            'SINGULARITY':                      '/usr/bin/singularity',
            'STARTER_LOG_NAME_APPEND':          'JobID',
            'STARTER_DEBUG':                    'D_CATEGORY D_SUBSECOND D_FULLDEBUG',
            'SHADOW_DEBUG':                     'D_CATEGORY D_SUBSECOND D_FULLDEBUG',
        },
    ) as the_condor:
        yield the_condor


@action
def the_job_handles(
    the_job_description, the_condor,
    test_dir, path_to_common_input, the_container_image
):
    job_handles = {}
    for name, case in TEST_CASES.items():
        submit_commands = {
            k: v.format(
                test_dir=test_dir,
                path_to_common_input=path_to_common_input,
                the_container_image=the_container_image,
            ) for k, v in case['submit_commands'].items()
        }
        description = {
            ** the_job_description,
            ** submit_commands,
        }
        handle = the_condor.submit(
            description=description,
            count=8,
        )

        job_handles[name] = handle

    return job_handles


@action(params={name: name for name in TEST_CASES})
def the_name_handle_pair(request, the_job_handles):
    return (request.param, the_job_handles[request.param])


@action
def the_case_name(the_name_handle_pair):
    return the_name_handle_pair[0]


@action
def the_job_handle(the_name_handle_pair):
    return the_name_handle_pair[1]


@action
def the_completed_job(the_condor, the_job_handle):
    assert the_job_handle.wait(
        timeout=120,
        condition=ClusterState.all_terminal,
        fail_condition=ClusterState.any_held,
    )
    return the_job_handle


@action
def the_expected_common_ads(the_case_name):
    return TEST_CASES[the_case_name]['expected_common_ads']


@action
def the_actual_common_ads(the_condor, the_completed_job):
    # The common ads were written before the job even started, so there
    # shouldn't be a race condition here.
    schedd = the_condor.get_local_schedd()
    return schedd.jobEpochHistory(
        ad_type=['common'],
        constraint=f"ClusterID == {the_completed_job.clusterid}",
    )


@action
def the_expected_epoch_ads(the_case_name):
    return TEST_CASES[the_case_name]['expected_epoch_ads']


@action
def the_actual_epoch_ads(the_condor, the_completed_job):
    # The epoch ads were written before the schedd exited, so we shouldn't
    # see the job-completed event before the epoch ads show up in the log.
    schedd = the_condor.get_local_schedd()
    return schedd.jobEpochHistory(
        ad_type=['epoch'],
        constraint=f"ClusterID == {the_completed_job.clusterid}",
    )


@action
def the_expected_cts_output(the_case_name, the_completed_job):
    return TEST_CASES[the_case_name]['expected_cts_output'].format(
        clusterID=the_completed_job.clusterid,
    );


@action
def the_actual_cts_output(the_condor, the_completed_job):
    r = the_condor.run_command(
        ['common_transfer_savings.py', str(the_completed_job.clusterid) ],
    )
    return r


def ad_contains(container, containee):
    for key in containee:
        if key not in container:
            return False
    return True


class TestCTS:
    def test_is_starting_condor(self,
        the_condor
    ):
        pass


    def test_job_is_running(self,
        the_completed_job
    ):
        pass


    def test_epoch_log_contents(self,
        the_completed_job,
        the_actual_common_ads, the_expected_common_ads,
        the_actual_epoch_ads, the_expected_epoch_ads,
    ):
        assert len(the_expected_common_ads) == len(the_actual_common_ads)
        for i in range(0, len(the_expected_common_ads)):
            assert ad_contains(
                the_actual_common_ads[i],
                the_expected_common_ads[i],
            )

        assert len(the_expected_epoch_ads) == len(the_actual_epoch_ads)
        for i in range(0, len(the_expected_epoch_ads)):
            assert ad_contains(
                the_actual_epoch_ads[i],
                the_expected_epoch_ads[i],
            )

    def test_common_transfer_savings(self,
        the_completed_job,
        the_actual_cts_output, the_expected_cts_output
    ):
        # Given that we're testing the tool, it's not a _setup_ error if
        # it doesn't exit successfully, so we have to check it here.
        assert the_actual_cts_output.returncode == 0
        assert the_actual_cts_output.stdout == the_expected_cts_output
