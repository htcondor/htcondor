#!/usr/bin/env pytest

from pathlib import Path

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

import htcondor

from ornithology import (
    action,
    config,
    Condor,
    ClusterState,
    JobStatus,
    format_script,
    write_file,
)


#
# FIXME: obviously, these could be parameterized.
#


@action
def test_script_contents():
    return format_script(
        """
        #!/usr/bin/env python3

        with open("new_output_file", "w") as f:
            f.write("")
        exit(0)
        """
    )


@action
def test_script(test_dir, test_script_contents):
    test_script = test_dir / "test_script.py"
    write_file(test_script, test_script_contents)
    return test_script


@action
def test_job_hash(test_dir, path_to_python, test_script):
    input_file = test_dir / "input_file"
    input_file.touch()
    return {
        "executable": path_to_python,
        "arguments": test_script,
        "transfer_executable": "true",
        "should_transfer_files": "true",
        "transfer_input_files": input_file,
        "universe": "vanilla",
        "log": "log",
        "output": "output",
        "error": "error",
    }


@action
def no_manifest_job_iwd(test_dir):
    iwd = test_dir / "completed_no_manifest_job"
    iwd.mkdir()
    return iwd


@action
def completed_no_manifest_job(no_manifest_job_iwd, default_condor, test_job_hash):
    cnmj = default_condor.submit({
        ** test_job_hash,
        "iwd": no_manifest_job_iwd,
        },
        count=1
    )
    assert cnmj.wait(
        condition=ClusterState.all_terminal,
        timeout=60,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    return cnmj


@action
def manifest_job_iwd(test_dir):
    iwd = test_dir / "completed_manifest_job"
    iwd.mkdir()
    return iwd


@action
def completed_manifest_job(manifest_job_iwd, default_condor, test_job_hash):
    cmj = default_condor.submit({
        ** test_job_hash,
        "iwd": manifest_job_iwd,
        "manifest": "True"
        },
        count=1
    )
    assert cmj.wait(
        condition=ClusterState.all_terminal,
        timeout=60,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    return cmj


@action
def manifest_dir_job_iwd(test_dir):
    iwd = test_dir / "completed_manifest_dir_job"
    iwd.mkdir()
    return iwd


@action
def completed_manifest_dir_job(manifest_dir_job_iwd, default_condor, test_job_hash):
    cmdj = default_condor.submit({
        ** test_job_hash,
        "iwd": manifest_dir_job_iwd,
        "manifest_dir": "custom-dir-name"
        },
        count=1
    )
    assert cmdj.wait(
        condition=ClusterState.all_terminal,
        timeout=60,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    return cmdj


class TestManifest:
    def test_no_manifest(self, completed_no_manifest_job, no_manifest_job_iwd):
        manifest_dir_name = "{}_0_manifest".format(completed_no_manifest_job.clusterid)
        manifest_dir = no_manifest_job_iwd / manifest_dir_name
        assert manifest_dir not in no_manifest_job_iwd.iterdir()


    # parameterize this and the following test
    def test_manifest_contents(self, completed_manifest_job, manifest_job_iwd):
        manifest_dir_name = "{}_0_manifest".format(completed_manifest_job.clusterid)
        manifest_dir = manifest_job_iwd / manifest_dir_name
        assert manifest_dir in manifest_job_iwd.iterdir()

        file_names = [p.name for p in manifest_dir.iterdir()]
        assert "out" in file_names
        assert "in" in file_names
        assert "environment" in file_names

        with (manifest_dir / "in").open() as f:
            assert "input_file\n" in f.readlines()
        with (manifest_dir / "out").open() as f:
            assert "new_output_file\n" in f.readlines()


    def test_manifest_dir_specified(self, completed_manifest_dir_job, manifest_dir_job_iwd):
        manifest_dir_name = "custom-dir-name"
        manifest_dir = manifest_dir_job_iwd / manifest_dir_name
        assert manifest_dir in manifest_dir_job_iwd.iterdir()

        file_names = [p.name for p in manifest_dir.iterdir()]
        assert "out" in file_names
        assert "in" in file_names
        assert "environment" in file_names

        with (manifest_dir / "in").open() as f:
            assert "input_file\n" in f.readlines()
        with (manifest_dir / "out").open() as f:
            assert "new_output_file\n" in f.readlines()

