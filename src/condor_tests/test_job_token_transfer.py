#!/usr/bin/env pytest

import logging
import os
import stat
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
tokenfiles=($@)
cat ${tokenfiles[@]/#/$_CONDOR_CREDS/}
ls $_CONDOR_CREDS >/dev/null 2>&1 || \
    >&2 echo 'Could not list $_CONDOR_CREDS:' "$(stat -L -c "%A %u %g" $_CONDOR_CREDS) (I am $(id -u) with group $(id -g))"
for tokenfile in $tokenfiles; do
    tokenpath="$_CONDOR_CREDS/$tokenfile"
    [ "$(id -u)" -eq "$(stat -L -c "%u" "$tokenpath")" ] || \
        >&2 echo "Bad ownership of $tokenfile: $(stat -L -c "%A %u %g" $tokenpath) (I am $(id -u) with group $(id -g))"
    tokenperms=$(stat -L -c "%a" "$tokenpath")
    [ "${tokenperms:0-2}" -eq "00" ] || \
        >&2 echo "Bad permissions on $tokenfile: $(stat -L -c "%A %u %g" $tokenpath) (I am $(id -u) with group $(id -g))"
done
"""

@standup
def write_job_hook_no_handles(test_dir):
    creds_no_handles = test_dir / "creds_no_handles.out"

    script_file = test_dir / "shadow_hook_no_handles.sh"
     # Check files and permissions for contents and current cred_dir
    script_contents = f"""#!/bin/bash
    for filepath in $_CONDOR_CREDS/* $_CONDOR_CREDS; do
        filename=$(basename "$filepath")
        stat -c '%A '"$filename" "$filepath"
    done > {creds_no_handles}
    exit 0"""
    script = open(script_file, "w")
    script.write(script_contents)
    script.close()
    st = os.stat(script_file)
    os.chmod(script_file, st.st_mode | stat.S_IEXEC)

@standup
def write_job_hook_two_handles(test_dir):
    creds_two_handles = test_dir / "creds_two_handles.out"

    script_file = test_dir / "shadow_hook_two_handles.sh"
    # Check files and permissions for contents and current cred_dir
    script_contents = f"""#!/bin/bash
    for filepath in $_CONDOR_CREDS/* $_CONDOR_CREDS; do
        filename=$(basename "$filepath")
        stat -c '%A '"$filename" "$filepath"
    done > {creds_two_handles}
    exit 0"""
    script = open(script_file, "w")
    script.write(script_contents)
    script.close()
    st = os.stat(script_file)
    os.chmod(script_file, st.st_mode | stat.S_IEXEC)

@standup
def condor(pytestconfig, test_dir, write_job_hook_no_handles, write_job_hook_two_handles):

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
            "CREDD_PORT":                        "-1",
            "TRUST_CREDENTIAL_DIRECTORY":        "True",
            "DUMMY_CLIENT_ID":                   "dummy_client_id",
            "DUMMY_CLIENT_SECRET_FILE":          str(secret_file),
            "DUMMY_RETURN_URL_SUFFIX":           "/return",
            "DUMMY_AUTHORIZATION_URL":           "https://$(FULL_HOSTNAME)/auth",
            "DUMMY_TOKEN_URL":                   "https://$(FULL_HOSTNAME)/token",
            "NONE_HOOK_SHADOW_PREPARE_JOB":      test_dir / "shadow_hook_no_handles.sh",
            "HANDLE_HOOK_SHADOW_PREPARE_JOB":    test_dir / "shadow_hook_two_handles.sh",
        },
    ) as condor:
        yield condor


@action
def vanilla_one_service_no_handles(condor, test_dir):
    logger.info("Running vanilla_one_service_no_handles")
    test_executable = test_dir / "test_wrapper.sh"
    outfile = test_dir / "vanilla_one_service_no_handles.out"
    errfile = outfile.with_suffix(".err")
    job = condor.submit(
        {
            "universe": "vanilla",
            "executable": str(test_executable),
            "arguments": "dummy.use",
            "use_oauth_services": "dummy",
            "output": str(outfile),
            "error": str(errfile),
            "log": str(outfile.with_suffix(".log")),
            "+HookKeyword": '"none"',
        }
    )
    assert job.wait(condition=ClusterState.all_terminal)
    return (outfile.open("r").read().strip(), errfile.open("r").read().strip())


@action
def vanilla_one_service_two_handles(condor, test_dir):
    logger.info("Running vanilla_one_service_two_handles")
    test_executable = test_dir / "test_wrapper.sh"
    outfile = test_dir / "vanilla_one_service_two_handles.out"
    errfile = outfile.with_suffix(".err")
    job = condor.submit(
        {
            "universe": "vanilla",
            "executable": str(test_executable),
            "arguments": "dummy_A.use dummy_B.use",
            "use_oauth_services": "dummy",
            "dummy_oauth_permissions_A": "",
            "dummy_oauth_permissions_B": "",
            "output": str(outfile),
            "error": str(errfile),
            "log": str(outfile.with_suffix(".log")),
            "+HookKeyword": '"handle"',
        }
    )
    assert job.wait(condition=ClusterState.all_terminal)
    return (outfile.open("r").read().strip(), errfile.open("r").read().strip())


@action
def local_one_service_no_handles(condor, test_dir):
    logger.info("Running local_one_service_no_handles")
    test_executable = test_dir / "test_wrapper.sh"
    outfile = test_dir / "local_one_service_no_handles.out"
    errfile = outfile.with_suffix(".err")
    job = condor.submit(
        {
            "universe": "local",
            "executable": str(test_executable),
            "arguments": "dummy.use",
            "use_oauth_services": "dummy",
            "output": str(outfile),
            "error": str(errfile),
            "log": str(outfile.with_suffix(".log")),
        }
    )
    assert job.wait(condition=ClusterState.all_terminal)
    return (outfile.open("r").read().strip(), errfile.open("r").read().strip())


@action
def local_one_service_two_handles(condor, test_dir):
    logger.info("Running local_one_service_two_handles")
    test_executable = test_dir / "test_wrapper.sh"
    outfile = test_dir / "local_one_service_two_handles.out"
    errfile = outfile.with_suffix(".err")
    job = condor.submit(
        {
            "universe": "local",
            "executable": str(test_executable),
            "arguments": "dummy_A.use dummy_B.use",
            "use_oauth_services": "dummy",
            "dummy_oauth_permissions_A": "",
            "dummy_oauth_permissions_B": "",
            "output": str(outfile),
            "error": str(errfile),
            "log": str(outfile.with_suffix(".log")),
        }
    )
    assert job.wait(condition=ClusterState.all_terminal)
    return (outfile.open("r").read().strip(), errfile.open("r").read().strip())


class TestJobTokenTransfer:

    def test_vanilla_one_service_no_handles(self, vanilla_one_service_no_handles):
        assert vanilla_one_service_no_handles[0] == "access_dummy"
        assert vanilla_one_service_no_handles[1] == ""

    def test_vanilla_one_service_two_handles(self, vanilla_one_service_two_handles):
        assert vanilla_one_service_two_handles[0] == "access_dummy_Aaccess_dummy_B"
        assert vanilla_one_service_two_handles[1] == ""

    def test_local_one_service_no_handles(self, local_one_service_no_handles):
        assert local_one_service_no_handles[0] == "access_dummy"
        assert local_one_service_no_handles[1] == ""

    def test_local_one_service_two_handles(self, local_one_service_two_handles):
        assert local_one_service_two_handles[0] == "access_dummy_Aaccess_dummy_B"
        assert local_one_service_two_handles[1] == ""

    # Below tests if shadow job hooks can read from the cred dir
    def test_shadow_hook_no_handles(self):
        with open('creds_no_handles.out') as f:
            log = f.read()
            assert "-r-------- dummy.use" in log
            assert "drwxr-xr-x _condor_creds.cluster1.proc0" in log

    def test_shadow_hook_two_handles(self):
        with open('creds_two_handles.out') as f:
            log = f.read()
            assert "-r-------- dummy_A.use" in log
            assert "-r-------- dummy_B.use" in log
            assert "drwxr-xr-x _condor_creds.cluster2.proc0" in log

