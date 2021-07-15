#!/usr/bin/env pytest

import logging
import os
from pathlib import Path
import stat
import subprocess

from ornithology import (
    config,
    standup,
    action,
    Condor,
    ClusterState,
    JobStatus
)


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@standup
def strace_condor_shadow(test_dir):
    script_file = test_dir / "strace_condor_shadow.sh"
    script_contents = f"""#!/bin/bash

exec strace -o {test_dir}/condor_shadow.strace.out -s 2000 condor_shadow "$@" """
    script = open(script_file, "w")
    script.write(script_contents)
    script.close()

    st = os.stat(script_file)
    os.chmod(script_file, st.st_mode | stat.S_IEXEC)


@standup
def condor(test_dir, strace_condor_shadow):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "SEC_DEFAULT_ENCRYPTION": "required",
            "SEC_DEFAULT_CRYPTO_METHODS": "AES",
            "SHADOW": test_dir / "strace_condor_shadow.sh",
            "SHADOW_DEBUG": "D_SECURITY:2",
            "STARTER_DEBUG": "D_SECURITY:2"
        },
    ) as condor:
        yield condor


@action
def write_transfer_file(test_dir):
    file_contents = "Hello, AES!"
    file = open(test_dir / "aes.txt", "w")
    file.write(file_contents)
    file.close()


@action
def transfer_filename(test_dir, write_transfer_file):
    return test_dir / "aes.txt"


@action
def shadow_log(test_dir):
    shadow_log = open(test_dir / "condor" / "log" / "ShadowLog", "r")
    contents = shadow_log.readlines()
    shadow_log.close()
    return contents


@action
def shadow_strace(test_dir):
    shadow_strace = open(test_dir / "condor_shadow.strace.out", "r")
    contents = shadow_strace.readlines()
    shadow_strace.close()
    return contents


@action
def aes_file_transfer_job(condor, transfer_filename, path_to_sleep, strace_condor_shadow):
    job = condor.submit(
        {
            "executable": path_to_sleep,
            "arguments": "1",
            "log": (condor.local_dir / "aes.log").as_posix(),
            "transfer_input_files": transfer_filename,
            "should_transfer_files": "YES",
        }
    )
    assert job.wait(condition=ClusterState.all_terminal)
    return job


class TestAESFileTransfer:
    def test_aes_file_transfer_job_succeeds(self, aes_file_transfer_job):
        assert aes_file_transfer_job.state[0] == JobStatus.COMPLETED

    def test_aes_file_transfer_job_file_contents_are_correct(
        self, aes_file_transfer_job, test_dir
    ):
        assert Path("aes.txt").read_text() == "Hello, AES!"

    def test_aes_in_shadow_log(
        self, aes_file_transfer_job, shadow_log
    ):
        aes_in_log = False
        for line in shadow_log:
            if "CRYPTO: protocol(AES), not clearing StreamCryptoState" in line:
                aes_in_log = True
                break
        assert aes_in_log

    def test_plaintext_in_strace(
        self, aes_file_transfer_job, shadow_strace
    ):
        plaintext_in_strace = False
        for line in shadow_strace:
            if "Hello, AES!" in line and "sendto" in line:
                plaintext_in_strace = True
                break
        assert plaintext_in_strace is False
