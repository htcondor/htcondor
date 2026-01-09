#!/usr/bin/env pytest

import pytest

from ornithology import (
    action,
    ClusterState,
    Condor,
    format_script,
    write_file,
)


@action
def path_to_common_file(test_dir):
    path = test_dir / "common.file"
    path.touch()
    return path


@action
def path_to_hook_log(test_dir):
    return test_dir / "prepare_job_hook.log"


@action
def path_to_prepare_job_hook(test_dir, path_to_hook_log):
    script_path = test_dir / "prepare_job_hook"

    script_text = format_script(
    f"""
        #!/bin/sh
        (
            pwd
            ls -la
        ) >> {path_to_hook_log.as_posix()}
    """
    )

    write_file(script_path, script_text)
    script_path.chmod(0o755)
    return script_path


@action
def the_condor(test_dir, path_to_prepare_job_hook):
    local_dir = test_dir / "condor"

    with Condor(
        local_dir=local_dir,
        config={
            "STARTER_JOB_HOOK_KEYWORD":             "TEST",
            "STARTER_DEFAULT_JOB_HOOK_KEYWORD":     "TEST",

            "TEST_HOOK_PREPARE_JOB":    path_to_prepare_job_hook.as_posix(),

            "NUM_CPUS":                 2,
        },
    ) as the_condor:
        yield the_condor


@action
def the_test_jobs(the_condor, test_dir, path_to_common_file):
    job_description = {
        'should_transfer_files':    True,
        "MY.CommonInputFiles":      f'"{path_to_common_file.as_posix()}"',

        'executable':               '/bin/sleep',
        'arguments':                30,
        'transfer_executable':      False,

        'log':                      (test_dir / 'test_job.log').as_posix(),

        'request_cpus':             1,
    }

    job_handle = the_condor.submit(
        description=job_description,
        count=2,
    )

    assert job_handle.wait(
        timeout=60,
        condition=ClusterState.all_complete,
    )

    return job_handle


class TestCIFPrepareJobHook:

    def test_job_hook_ran_after_common_file_transfer(self, the_test_jobs, path_to_hook_log, path_to_common_file):
        sif_file_count = 0
        for line in path_to_hook_log.read_text().splitlines():
            if path_to_common_file.name in line:
                sif_file_count +=1
        assert(sif_file_count == 2)
