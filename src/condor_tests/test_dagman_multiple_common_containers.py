#!/usr/bin/env pytest

#
# Test that a DAG which contains two different containers works when
# CONTAINER_IMAGES_COMMON_BY_DEFAULT is TRUE.
#
# FIXME:
# This does NOT test what happens if the containers have the same basename.
#

import pytest

from libcontainer import (
    SingularityIsWorthy,
    UserNamespacesFunctional,
    SingularityIsWorking,
    make_empty_sif,
    EMPTY_SIF_BIND_EXPR,
)

from ornithology import (
    action,
    Condor,
    ClusterState,
    ChangeDir,
)

import htcondor2


# ---- Constants --------------------------------------------------------------

DAG_TEXT = """\
    JOB A dag-test-a.submit
    JOB B dag-test-a.submit
    JOB C dag-test-a.submit
    JOB D dag-test-a.submit

    JOB E dag-test-b.submit
    JOB F dag-test-b.submit
    JOB G dag-test-b.submit
    JOB H dag-test-b.submit
"""

SUBMIT_X_TEXT = """\
    universe = container
    shell = ls -la > output-file-$(JOB); cat common-file input-file-$(JOB) >> output-file-$(JOB); exit 0
    transfer_input_files = input-file-$(JOB)
    transfer_output_files = output-file-$(JOB)
    should_transfer_files = YES
    request_cpus = 1
    request_memory = 1024
    request_disk = 2048
    log = log.$(JOB)

    container_image = {container_image}
    MY.CommonInputFiles = "common-file"

    queue 1
"""


# -----------------------------------------------------------------------------


@action
def container_image_a(test_dir):
    sif = make_empty_sif(test_dir / "empty-a.sif")
    assert sif is not None
    return sif


@action
def container_image_b(test_dir):
    sif = make_empty_sif(test_dir / "empty-b.sif")
    assert sif is not None
    return sif


@action
def the_dag_dir(test_dir, container_image_a, container_image_b):
    dag_dir = test_dir / "the_test_dir"
    dag_dir.mkdir()

    (dag_dir / "dag-test-a.submit").write_text(
        SUBMIT_X_TEXT.format(container_image=container_image_a)
    )
    (dag_dir / "dag-test-b.submit").write_text(
        SUBMIT_X_TEXT.format(container_image=container_image_b)
    )
    for letter in "ABCDEFGH":
        (dag_dir / f"input-file-{letter}").write_text(letter)
    (dag_dir / "common-file").write_text("the common input file")

    dag = dag_dir / "the_dag.dag"
    dag.write_text(DAG_TEXT)

    return dag_dir


@action
def the_condor(test_dir):
    with Condor(
        local_dir = test_dir / "the_condor",
        config = {
            "SINGULARITY_TEST_SANDBOX_TIMEOUT":     "50",
            "SINGULARITY":                          "/usr/bin/singularity",
            'SINGULARITY_BIND_EXPR':                f'"{EMPTY_SIF_BIND_EXPR}"',
            "CONTAINER_IMAGES_COMMON_BY_DEFAULT":   "True",
        },
    ) as condor:
        yield condor


@action
def the_completed_dag(the_condor, the_dag_dir):
    with ChangeDir(the_dag_dir):
        dag = htcondor2.Submit.from_dag(str(the_dag_dir/"the_dag.dag"))
        handle = the_condor.submit(dag)

    # If the DAG doesn't finish, there's no way the jobs did.
    assert handle.wait(
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
        timeout=300,
    )

    return handle


class TestDAGManMultipleCommonFiles:

    @pytest.mark.skipif(not SingularityIsWorthy(), reason="No worthy Singularity/Apptainer found")
    @pytest.mark.skipif(not UserNamespacesFunctional(), reason="User namespaces not working -- some limit hit?")
    @pytest.mark.skipif(not SingularityIsWorking(), reason="Singularity doesn't seem to be working")
    def test_success(self, the_dag_dir, the_completed_dag):
        for letter in "ABCDEFGH":
            print( f"Checking job `{letter}`..." )
            log = the_dag_dir / f"log.{letter}"
            assert log.exists()

            job_terminated_successfully = False
            job_transferred_common_files = False

            jel = htcondor2.JobEventLog(str(log))
            for event in jel.events(stop_after=0):
                if event.type is htcondor2.JobEventType.JOB_TERMINATED:
                    if event['TerminatedNormally'] is True:
                        if event['ReturnValue'] == 0:
                            job_terminated_successfully = True
                if event.type is htcondor2.JobEventType.COMMON_FILES:
                    if event['Type'] == "TransferFinished":
                        job_transferred_common_files = True

            assert(job_terminated_successfully)
            assert(job_transferred_common_files)
