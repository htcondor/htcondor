#!/usr/bin/env pytest

from pathlib import Path
from getpass import getuser

from ornithology import (
    config,
    standup,
    action,
    Condor,
)

import htcondor2
htcondor2.enable_debug()

import logging


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


TEST_CASES = {
    "local_issuer": {
        "config": {
            "SEC_CREDENTIAL_DIRECTORY_OAUTH":       "$(LOCAL_DIR)/oauth_credentials",
            "CREDMON_OAUTH_LOG":                    "$(LOG)/CredMonOAuthLog",
            "DAEMON_LIST":                          "$(DAEMON_LIST),CREDMON_OAUTH",
            "AUTO_INCLUDE_CREDD_IN_DAEMON_LIST":    "True",
            "TRUST_CREDENTIAL_DIRECTORY":           "True",
            "LOCAL_CREDMON_PROVIDER_NAME":          "scitokens",
            "LOCAL_CREDMON_TOKEN_AUDIENCE":         "https://localhost",
            "ALLOW_DAEMON":                         "*",
            "LOCAL_CREDMON_PRIVATE_KEY":            "$(LOCAL_DIR)/trust_domain_ca_privkey.pem",
            "CREDD_DEBUG":                          "D_FULLDEBUG",
        },
        "the_directory":    "{local_dir}/oauth_credentials",
        "the_local_filename":     "scitokens.top",
        "the_plain_filename":     "plain.use",
    },
}


@action(params={name: name for name in TEST_CASES})
def the_test_tuple(request):
    return (request.param, TEST_CASES[request.param])


@action
def the_test_name(the_test_tuple):
    return the_test_tuple[0]


@action
def the_test_case(the_test_tuple):
    return the_test_tuple[1]


@action
def the_local_dir(the_test_name, test_dir):
    return Path(str(test_dir)) / the_test_name


@action
def the_directory(the_test_case, the_local_dir):
    return the_test_case['the_directory'].format(local_dir=the_local_dir)


@action
def the_username():
    # Seems like we should have a way to get this from HTCondor.
    return getuser()


@action
def the_local_filename(the_test_case):
    return the_test_case['the_local_filename']


@action
def the_plain_filename(the_test_case):
    return the_test_case['the_plain_filename']


@action
def fake_user_token_file(test_dir):
    token_file = test_dir / "the_fake_use_token"
    token_file.write_text("fake-user-token")
    return token_file


@action
def the_condor(the_test_case, the_local_dir):
    with Condor(
        local_dir=the_local_dir,
        config=the_test_case['config']
    ) as the_condor:
       yield the_condor


class TestIssueCredentials:

    def test_top_file_created(self, the_test_name, the_condor, the_directory, the_username, the_local_filename):
        # Strictly speaking, this would be a set-up error.
        assert not (Path(the_directory) / the_username / the_local_filename).exists()

        # Strictly speaking, this should be an @action.
        submit = htcondor2.Submit(
                f"""
                executable = /bin/sleep
                transfer_executable = false
                arguments = 5

                log = {the_test_name}.log
                """
        )
        with the_condor.use_config():
            submit.issue_credentials()

        # Check for a .use file as well?
        assert (Path(the_directory) / the_username / the_local_filename).exists()

    # Test that a user-added credential that's not managed by any credmons
    # results in a .use file in the credd's storage.
    def test_use_file_created(self, the_test_name, the_condor, the_directory, the_username, the_plain_filename, fake_user_token_file):
        # Strictly speaking, this would be a set-up error.
        assert not (Path(the_directory) / the_username / the_plain_filename).exists()

        rv = the_condor.run_command(
            ['htcondor', '-v', 'credential', 'add', 'oauth2',
             str(fake_user_token_file),
             '--service', 'plain'],
        )
        assert rv.returncode == 0
        logger.info(rv.stdout)
        logger.info(rv.stderr)

        assert (Path(the_directory) / the_username / the_plain_filename).exists()
