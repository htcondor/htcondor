#!/usr/bin/env pytest

import pytest
import subprocess

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

from ornithology import (
    Condor,
    action,
    ClusterState,
)

from pathlib import Path

from libcontainer import (
    SingularityIsWorthy,
    UserNamespacesFunctional,
    SingularityIsWorking,
    make_empty_sif,
    EMPTY_SIF_BIND_EXPR,
)

import htcondor2


#
# Our other tests don't check to see if common file transfer happens if we
# recycle a shadow.  Since it doesn't, test_common_transfer_savings.py would
# fail.  Either none of our other tests recycle a shadow, or they're only
# checking for correctness, and jobs correctly fall back to uncommon file
# transfer.
#
# In either case, this test verifies that the number of common transfers is
# correct; due to race conditions, we may to have to correct it to determine
# if the number of common transfers it no less than the correct number.
#
# Additionally, this test should confirm that case it was looking for
# actually happened, which we'll do by checking the schedd log.
#


@action
def the_container_image(test_dir, pytestconfig):
    sif = make_empty_sif(test_dir / "busybox.sif")
    assert sif is not None
    return sif


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
        'fall_back_transfers':  0,
    },

    # We'll skip to the most complicated test case; we can fill in the other
    # cases if this one fails but none of the other ones do.
    #
    # (As a result of sequential common file transfer, this test no longer
    # transfers each catalog twice; if that's desirable, see one of the other
    # test cases for how to arrange it (KEEP_DATA_CLAIM_IDLE).)
    "three_cxfers_2_double": {
        'submit_commands':      {
            'MY.CommonInputFiles':              '"{path_to_common_input}, file://{path_to_common_input}.0"',

            'container_image':                  'file://{the_container_image}',
            'container_is_common':              'True',

            'MY.CommonInputCatalogs':           '"A, B, container_busybox_sif"',
            'MY._x_catalog_A':                  '"file://{path_to_common_input}.1, {path_to_common_input}.2"',
            'MY._x_catalog_B':                  '"file://{path_to_common_input}.3, {path_to_common_input}.4"',
        },
        'expected_common_ads':   [
            {'CommonPluginResultList'},
            {'CommonPluginResultList'},
            {'CommonPluginResultList'},
            {'CommonPluginResultList'},
        ],
        'expected_epoch_ads':  [
            {'CommonFilesMappedTime', 'CommonInputCatalogs', '_x_catalog_container_busybox_sif', 'CommonInputFiles'},
            {'CommonFilesMappedTime', 'CommonInputCatalogs', '_x_catalog_container_busybox_sif', 'CommonInputFiles'},
            {'CommonFilesMappedTime', 'CommonInputCatalogs', '_x_catalog_container_busybox_sif', 'CommonInputFiles'},
            {'CommonFilesMappedTime', 'CommonInputCatalogs', '_x_catalog_container_busybox_sif', 'CommonInputFiles'},
            {'CommonFilesMappedTime', 'CommonInputCatalogs', '_x_catalog_container_busybox_sif', 'CommonInputFiles'},
            {'CommonFilesMappedTime', 'CommonInputCatalogs', '_x_catalog_container_busybox_sif', 'CommonInputFiles'},
            {'CommonFilesMappedTime', 'CommonInputCatalogs', '_x_catalog_container_busybox_sif', 'CommonInputFiles'},
            {'CommonFilesMappedTime', 'CommonInputCatalogs', '_x_catalog_container_busybox_sif', 'CommonInputFiles'},
        ],
    },
}


@action
def path_to_common_input(test_dir):
    path = test_dir / 'common-input.file'
    path.write_text('the common input file')

    for i in range(0, 6):
        new_path = test_dir / f"common-input.file.{i}"
        new_path.write_text(f'{i}')

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
            "sleep 10",
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
            'STARTER_TEST_SANDBOX_TIMEOUT':     50,
            'SINGULARITY':                      '/usr/bin/singularity',
            'SINGULARITY_BIND_EXPR':            f'"{EMPTY_SIF_BIND_EXPR}"',
            'STARTER_LOG_NAME_APPEND':          'JobID',
            'STARTER_DEBUG':                    'D_CATEGORY D_SUBSECOND D_TEST',
            'SHADOW_DEBUG':                     'D_CATEGORY D_SUBSECOND D_TEST',
            'SCHEDD_DEBUG':                     'D_CATEGORY D_SUBSECOND D_TEST',
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


def ad_contains(container, containee):
    for key in containee:
        if key not in container:
            return False
    return True


# -----------------------------------------------------------------------------


# We don't need apptainer for _all_ of the test cases, but I don't know how
# to skip cases rather than tests.
@pytest.mark.skipif(
    not SingularityIsWorthy(),
    reason="No worthy Singularity/Apptainer found"
)
@pytest.mark.skipif(
    not UserNamespacesFunctional(),
    reason="User namespaces not working -- some limit hit?"
)
@pytest.mark.skipif(
    not SingularityIsWorking(),
    reason="Singularity doesn't seem to be working"
)
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


    def test_schedd_log_contents(self,
        the_condor,
        the_completed_job,
        the_actual_common_ads, the_expected_common_ads,
        the_actual_epoch_ads, the_expected_epoch_ads,
    ):
        schedd_attempted_to_recycle_shadow = False
        schedd_log = the_condor.schedd_log.open()
        for line in schedd_log.read():
            if line.message == "RecycleShadow(): will recycle if runnable job found for claim.":
                schedd_attempted_to_recycle_shadow = True
                break
        assert schedd_attempted_to_recycle_shadow
