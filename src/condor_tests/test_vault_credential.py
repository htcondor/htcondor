#!/usr/bin/env pytest

import htcondor2

import logging
from getpass import getuser

from ornithology import *


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


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
            DAEMON_LIST                         = $(DAEMON_LIST),CREDD
            TRUSTED_VAULT_HOSTS                 = trusted.org, alsotrusted.edu
        """,
    ) as the_condor:
        yield the_condor


@action
def trusted_token_file(test_dir):
    token_file = test_dir / "the_trusted_token"
    token_file.write_text('{"vault_token": "XXX", "vault_url": "https://trusted.org"}')
    return token_file


@action
def untrusted_token_file(test_dir):
    token_file = test_dir / "the_untrusted_token"
    token_file.write_text('{"vault_token": "XXX", "vault_url": "https://untrusted.org"}')
    return token_file


class TestVaultCredential:

    def test_trusted_credential(self, local_dir, the_condor, trusted_token_file):
        rv = the_condor.run_command(
            ['condor_store_cred', 'add-oauth', '-s', 'vault1',
             '-i', str(trusted_token_file)],
        )
        logger.info(rv.stdout)
        logger.info(rv.stderr)
        assert rv.returncode == 0

        top_file = local_dir / "oauth.d" / getuser() / "vault1.top"
        assert trusted_token_file.read_text() == top_file.read_text()

    def test_untrusted_credential(self, local_dir, the_condor, untrusted_token_file):
        rv = the_condor.run_command(
            ['condor_store_cred', 'add-oauth', '-s', 'vault2',
             '-i', str(untrusted_token_file)],
        )
        logger.info(rv.stdout)
        logger.info(rv.stderr)
        assert rv.returncode != 0
