#!/usr/bin/env pytest

from pathlib import Path

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

from ornithology import (
    action,
    config,
    Condor,
    ClusterState,
    JobStatus,
    format_script,
    write_file,
)


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


TEST_PARAMETERS = {
    "no_manifest": {
        "iwd": "completed_no_manifest_job",
        "job_attrs": {},
        "manifest_dir_name": None,
    },
    "manifest": {
        "iwd": "completed_manifest_job",
        "job_attrs": {"manifest": "True",},
        "manifest_dir_name": "{}_0_manifest",
    },
    "manifest_dir": {
        "iwd": "completed_manifest_dir",
        "job_attrs": {"manifest_dir": "custom-dir-name",},
        "manifest_dir_name": "custom-dir-name",
    },
}


@action(params=TEST_PARAMETERS)
def test_parameters(request):
    return request.param


@action
def test_job_iwd(test_dir, test_parameters):
    iwd = test_dir / test_parameters["iwd"]
    iwd.mkdir()
    return iwd


@action
def completed_test_job(test_job_iwd, default_condor, test_job_hash, test_parameters):
    ctj = default_condor.submit(
        {**test_job_hash, **test_parameters["job_attrs"], "iwd": test_job_iwd,}, count=1
    )
    assert ctj.wait(
        condition=ClusterState.all_terminal,
        timeout=60,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    return ctj


class TestManifest:
    def test_manifest_contents(self, completed_test_job, test_job_iwd, test_parameters):
        # This is a little clumsy, but probably better than duplicating
        # test_job_iwd and completed_test_job to avoid a conditional.
        if test_parameters["manifest_dir_name"] is None:
            manifest_dir_name = "{}_0_manifest".format(completed_test_job.clusterid)
            manifest_dir = test_job_iwd / manifest_dir_name
            assert manifest_dir not in test_job_iwd.iterdir()
            return

        manifest_dir_name = test_parameters["manifest_dir_name"].format(
            completed_test_job.clusterid
        )
        manifest_dir = test_job_iwd / manifest_dir_name
        assert manifest_dir in test_job_iwd.iterdir()

        file_names = [p.name for p in manifest_dir.iterdir()]
        assert "out" in file_names
        assert "in" in file_names
        assert "environment" in file_names

        with (manifest_dir / "in").open() as f:
            assert "./input_file\n" in f.readlines()
        with (manifest_dir / "out").open() as f:
            assert "./new_output_file\n" in f.readlines()
