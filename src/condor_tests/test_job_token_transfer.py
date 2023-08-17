#!/usr/bin/env pytest

import logging
from getpass import getuser
from ornithology import (
    standup,
    action,
    Condor,
    ClusterState
)


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


USER = getuser()


# This test scripts prints out the cat'd contents
# of $_CONDOR_CREDS/{$1,$2,...,$n}
TEST_SCRIPT = """#!/bin/bash
args=($@)
cat ${args[@]/#/$_CONDOR_CREDS/}
"""


@standup
def condor(pytestconfig, test_dir):

    logger.info("Preparing credmon")
    credmon = pytestconfig.invocation_dir / "condor_credmon_oauth_dummy"
    credmon.chmod(0o755)  # make sure the credmon is executable

    logger.info("Preparing top-level credentials directory")
    cred_dir = test_dir / "oauth_credentials"
    cred_dir.mkdir(parents=True, exist_ok=True)
    cred_dir.chmod(0o2770)

    logger.info("Preparing user credential directory")
    user_cred_dir = cred_dir / USER
    user_cred_dir.mkdir(parents=True, exist_ok=True)

    logger.info("Creating fake user credentials")
    for token_name in ["dummy", "dummy_A", "dummy_B"]:
        (user_cred_dir / token_name).with_suffix(".top").open("w").write(f"refresh_{token_name}")
        (user_cred_dir / token_name).with_suffix(".use").open("w").write(f"access_{token_name}")

    logger.info("Creating fake OAuth secret")
    secret_file = test_dir / "dummy.secret"
    secret_file.open("w").write("dummy_secret")
    secret_file.chmod(0o600)

    logger.info("Creating test wrapper script")
    test_executable = test_dir / "test_wrapper.sh"
    test_executable.open("w").write(TEST_SCRIPT)
    test_executable.chmod(0o755)

    logger.info("Setting up HTCondor")
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "SEC_DEFAULT_ENCRYPTION":            "REQUIRED",
            "SEC_CREDENTIAL_DIRECTORY_OAUTH":    str(cred_dir),
            "CREDMON_OAUTH":                     str(credmon),
            "CREDMON_OAUTH_LOG":                 "$(LOG)/CredMonOAuthLog",
            "DAEMON_LIST":                       "$(DAEMON_LIST) CREDMON_OAUTH",
            "AUTO_INCLUDE_CREDD_IN_DAEMON_LIST": "True",
            "TRUST_CREDENTIAL_DIRECTORY":        "True",
            "DUMMY_CLIENT_ID":                   "dummy_client_id",
            "DUMMY_CLIENT_SECRET_FILE":          str(secret_file),
            "DUMMY_RETURN_URL_SUFFIX":           "/return",
            "DUMMY_AUTHORIZATION_URL":           "https://$(FULL_HOSTNAME)/auth",
            "DUMMY_TOKEN_URL":                   "https://$(FULL_HOSTNAME)/token",
        },
    ) as condor:
        yield condor


@action
def vanilla_one_service_no_handles(condor, test_dir):
    logger.info("Running vanilla_one_service_no_handles")
    test_executable = test_dir / "test_wrapper.sh"
    outfile = test_dir / "vanilla_one_service_no_handles.out"
    job = condor.submit(
        {
            "universe": "vanilla",
            "executable": str(test_executable),
            "arguments": "dummy.use",
            "use_oauth_services": "dummy",
            "output": str(outfile),
            "error": str(outfile.with_suffix(".err")),
            "log": str(outfile.with_suffix(".log")),
        }
    )
    assert job.wait(condition=ClusterState.all_terminal)
    return outfile.open("r").read().strip()


@action
def vanilla_one_service_two_handles(condor, test_dir):
    logger.info("Running vanilla_one_service_two_handles")
    test_executable = test_dir / "test_wrapper.sh"
    outfile = test_dir / "vanilla_one_service_two_handles.out"
    job = condor.submit(
        {
            "universe": "vanilla",
            "executable": str(test_executable),
            "arguments": "dummy_A.use dummy_B.use",
            "use_oauth_services": "dummy",
            "dummy_oauth_permissions_A": "",
            "dummy_oauth_permissions_B": "",
            "output": str(outfile),
            "error": str(outfile.with_suffix(".err")),
            "log": str(outfile.with_suffix(".log")),
        }
    )
    assert job.wait(condition=ClusterState.all_terminal)
    return outfile.open("r").read().strip()


@action
def local_one_service_no_handles(condor, test_dir):
    logger.info("Running local_one_service_no_handles")
    test_executable = test_dir / "test_wrapper.sh"
    outfile = test_dir / "local_one_service_no_handles.out"
    job = condor.submit(
        {
            "universe": "local",
            "executable": str(test_executable),
            "arguments": "dummy.use",
            "use_oauth_services": "dummy",
            "output": str(outfile),
            "error": str(outfile.with_suffix(".err")),
            "log": str(outfile.with_suffix(".log")),
        }
    )
    assert job.wait(condition=ClusterState.all_terminal)
    return outfile.open("r").read().strip()


@action
def local_one_service_two_handles(condor, test_dir):
    logger.info("Running local_one_service_two_handles")
    test_executable = test_dir / "test_wrapper.sh"
    outfile = test_dir / "local_one_service_two_handles.out"
    job = condor.submit(
        {
            "universe": "local",
            "executable": str(test_executable),
            "arguments": "dummy_A.use dummy_B.use",
            "use_oauth_services": "dummy",
            "dummy_oauth_permissions_A": "",
            "dummy_oauth_permissions_B": "",
            "output": str(outfile),
            "error": str(outfile.with_suffix(".err")),
            "log": str(outfile.with_suffix(".log")),
        }
    )
    assert job.wait(condition=ClusterState.all_terminal)
    return outfile.open("r").read().strip()


@action
def scheduler_one_service_no_handles(condor, test_dir):
    logger.info("Running scheduler_one_service_no_handles")
    test_executable = test_dir / "test_wrapper.sh"
    outfile = test_dir / "scheduler_one_service_no_handles.out"
    job = condor.submit(
        {
            "universe": "scheduler",
            "executable": str(test_executable),
            "arguments": "dummy.use",
            "use_oauth_services": "dummy",
            "output": str(outfile),
            "error": str(outfile.with_suffix(".err")),
            "log": str(outfile.with_suffix(".log")),
        }
    )
    assert job.wait(condition=ClusterState.all_terminal)
    return outfile.open("r").read().strip()


@action
def scheduler_one_service_two_handles(condor, test_dir):
    logger.info("Running scheduler_one_service_two_handles")
    test_executable = test_dir / "test_wrapper.sh"
    outfile = test_dir / "scheduler_one_service_two_handles.out"
    job = condor.submit(
        {
            "universe": "scheduler",
            "executable": str(test_executable),
            "arguments": "dummy_A.use dummy_B.use",
            "use_oauth_services": "dummy",
            "dummy_oauth_permissions_A": "",
            "dummy_oauth_permissions_B": "",
            "output": str(outfile),
            "error": str(outfile.with_suffix(".err")),
            "log": str(outfile.with_suffix(".log")),
        }
    )
    assert job.wait(condition=ClusterState.all_terminal)
    return outfile.open("r").read().strip()


class TestJobTokenTransfer:

    def test_vanilla_one_service_no_handles(self, vanilla_one_service_no_handles):
        assert vanilla_one_service_no_handles == "access_dummy"

    def test_vanilla_one_service_two_handles(self, vanilla_one_service_two_handles):
        assert vanilla_one_service_two_handles == "access_dummy_Aaccess_dummy_B"

    def test_local_one_service_no_handles(self, local_one_service_no_handles):
        assert local_one_service_no_handles == "access_dummy"

    def test_local_one_service_two_handles(self, local_one_service_two_handles):
        assert local_one_service_two_handles == "access_dummy_Aaccess_dummy_B"

    def test_scheduler_one_service_no_handles(self, scheduler_one_service_no_handles):
        assert scheduler_one_service_no_handles == "access_dummy"

    def test_scheduler_one_service_two_handles(self, scheduler_one_service_two_handles):
        assert scheduler_one_service_two_handles == "access_dummy_Aaccess_dummy_B"
