#!/usr/bin/env pytest

import os
import logging
import platform
import time
import htcondor2 as htcondor

from ornithology import (
    config,
    standup,
    action,
    Condor,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

@standup
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "COLLECTOR.ALLOW_ADMINISTRATOR": "$(ALLOW_ADMINISTRATOR) joe@here",
            "MASTER_NAME": "test@here",
            "STARTD_NAME": "test@here",
        },
    ) as condor:
        yield condor

@action
def token(condor):
    rv = condor.run_command(
        ["condor_token_create", "-identity", "joe@here", "-token", "joe"]
    )
    assert rv.returncode == 0
    return True

class TestAdminCapability:
    def test_reconfig(self, condor, token):
        # Ugh. The master waits for 5 seconds before sending its ad to
        # the collector, and Ornithology doesn't wait for the master ad
        # to appear before assuming the installation is up and ready for
        # use.
        time.sleep(5)

        # Override SEC_PASSWORD_DIRECTORY so that condor_reconfig doesn't
        #   have access to the POOL signing key.
        # Override SEC_CLIENT_AUTHENTICATION_METHODS so that our special
        #   IDToken is used for authentication.
        rv = condor.run_command(
            ["env", "_condor_SEC_PASSWORD_DIRECTORY=/nothere",
                "_condor_SEC_CLIENT_AUTHENTICATION_METHODS=IDTOKEN",
                "condor_reconfig", "-name", "test@here"]
        )
        assert rv.returncode == 0

    def test_drain(self, condor, token):
        # Override SEC_PASSWORD_DIRECTORY so that condor_drain doesn't
        #   have access to the POOL signing key.
        # Override SEC_CLIENT_AUTHENTICATION_METHODS so that our special
        #   IDToken is used for authentication.
        # Override STARTD_ADDRESS_FILE so that condor_drain has to
        #   query the collector to locate the startd.
        rv = condor.run_command(
            ["env", "_condor_SEC_PASSWORD_DIRECTORY=/nothere",
                "_condor_SEC_CLIENT_AUTHENTICATION_METHODS=IDTOKEN",
                "_condor_STARTD_ADDRESS_FILE=/nothere",
                "condor_drain", "-debug", "slot1@test@here"]
        )
        assert rv.returncode == 0
