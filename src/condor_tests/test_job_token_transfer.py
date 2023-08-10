#!/usr/bin/env pytest

import logging
from getpass import getuser
from pathlib import Path
from ornithology import (
    config,
    standup,
    action,
    Condor,
    ClusterState
)


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


USER = getuser()


@standup
def condor(pytestconfig, test_dir):
    credmon = pytestconfig.invocation_dir / "condor_credmon_oauth_dummy"
    credmon.chmod(0o755)  # make sure the credmon is executable
    cred_dir = test_dir / "oauth_credentials"
    cred_dir.mkdir(parents=True, exist_ok=True)
    cred_dir.chmod(0o2770)
    user_cred_dir = cred_dir / USER
    user_cred_dir.mkdir(parents=True, exist_ok=True)
    secret_file = test_dir / "dummy.secret"
    secret_file.open("w").write("dummy_secret")
    secret_file.chmod(0o600)
    for token_name in ["dummy", "dummy_A", "dummy_B"]:
        (user_cred_dir / token_name).with_suffix(".top").open("w").write(f"refresh_{token_name}")
        (user_cred_dir / token_name).with_suffix(".use").open("w").write(f"access_{token_name}")
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
def one_service_no_handles(condor, test_dir):
    outfile = test_dir / "one_service_no_handles.out"
    job = condor.submit(
        {
            "executable": "/bin/cat",
            "arguments": ".condor_creds/dummy.use",
            "should_transfer_files": "yes",
            "use_oauth_services": "dummy",
            "output": str(outfile),
            "error": str(outfile.with_suffix(".err")),
            "log": str(outfile.with_suffix(".log")),
        }
    )
    assert job.wait(condition=ClusterState.all_terminal)
    return outfile.open("r").read().strip()


@action
def one_service_two_handles(condor, test_dir):
    outfile = test_dir / "one_service_two_handles.out"
    job = condor.submit(
        {
            "executable": "/bin/cat",
            "arguments": ".condor_creds/dummy_A.use .condor_creds/dummy_B.use",
            "should_transfer_files": "yes",
            "use_oauth_services": "dummy",
            "dummy_oauth_permissions_A": "",
            "dummy_oauth_permissions_B": "",
            "output": str(outfile),
            "error": str(outfile.with_suffix(".err")),
            "log": str(outfile.with_suffix(".log")),
        }
    )
    assert job.wait(condition=ClusterState.all_terminal)
    for line in outfile.open("r"):
        yield line.strip()


class TestJobTokenTransfer:

    def test_one_service_no_handles(self, one_service_no_handles):
        assert one_service_no_handles == "access_dummy"

    def test_one_service_two_handles(self, one_service_two_handles):
        assert tuple(one_service_no_handles) == ("access_dummy_A", "access_dummy_B")
