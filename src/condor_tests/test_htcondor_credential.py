#!/usr/bin/env pytest

import htcondor2

import logging
from getpass import getuser

from ornithology import *


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


#
# This test is woefully incomplete: it _assumes_ that the various credential
# verbs have their intended effect, or are effected as intended, on (or by)
# the rest of the system.  Elimnating that assumption would be a lot of work,
# but would also make it possible to write verb-specific tests -- although
# the complete set of tests would still need to check that the command-line
# tool works properly with itself.
#


@action
def local_dir(test_dir):
    return test_dir / "condor"


@action
def the_condor(local_dir):
    with Condor(
        local_dir=local_dir,
        raw_config="""
            # Enable OAuth2.
            SEC_CREDENTIAL_DIRECTORY_OAUTH      = $(LOCAL_DIR)/oauth.d
            CREDMON_OAUTH_LOG                   = $(LOG)/CredMonOAuthLog
            DAEMON_LIST                         = $(DAEMON_LIST),CREDMON_OAUTH
            AUTO_INCLUDE_CREDD_IN_DAEMON_LIST   = True
            TRUST_CREDENTIAL_DIRECTORY          = True
            LOCAL_CREDMON_PROVIDER_NAME         = scitokens
            LOCAL_CREDMON_TOKEN_AUDIENCE        = https://localhost
            CREDD_PORT                          = -1
            ALLOW_DAEMON                        = *
            LOCAL_CREDMON_PRIVATE_KEY           = $(LOCAL_DIR)/trust_domain_ca_privkey.pem
        """,
    ) as the_condor:
        yield the_condor


@action
def fake_refresh_token_file(test_dir):
    token_file = test_dir / "the_fake_refresh_token"
    token_file.write_text("fake-refresh-token")
    return token_file


class TestHTCondorCredential:

    def test_credential_get(self, local_dir, the_condor, fake_refresh_token_file):
        rv = the_condor.run_command(
            ['htcondor', '-v', 'credential', 'add', 'oauth2',
             str(fake_refresh_token_file),
             '--service', 'scitokens'],
        )
        assert rv.returncode == 0
        logger.info(rv.stdout)
        logger.info(rv.stderr)

        daemon_log = the_condor.credmon_oauth_log.open()
        assert daemon_log.wait(
            condition=lambda line: "Successfully renewed" in line.message,
            timeout=20,
        )

        rv = the_condor.run_command(
            ['htcondor', '-v', 'credential', 'get', 'oauth2',
            '--service', 'scitokens'],
        )
        assert rv.returncode == 0
        logger.info(rv.stdout)
        logger.info(rv.stderr)

        use_file = local_dir / "oauth.d" / getuser() / "scitokens.use"
        assert rv.stdout == use_file.read_text()
