#!/usr/bin/env pytest

import classad
import htcondor
import logging
import os
import shutil
import subprocess
import time

from ornithology import *
from pathlib import Path

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@standup
def config_dir(test_dir):
    config_dir = test_dir / "condor" / "config"
    Path(config_dir).mkdir(parents=True, exist_ok=True)
    return config_dir


@standup
def user_tokens_dir(test_dir):
    user_tokens_dir = test_dir / "condor" / "user-tokens.d"
    Path(user_tokens_dir).mkdir(parents=True, exist_ok=True)
    return user_tokens_dir


@standup
def system_tokens_dir(test_dir):
    system_tokens_dir = test_dir / "condor" / "system-tokens.d"
    Path(system_tokens_dir).mkdir(parents=True, exist_ok=True)
    return system_tokens_dir


@standup
def passwords_dir(test_dir):
    passwords_dir = test_dir / "condor" / "password.d"
    Path(passwords_dir).mkdir(parents=True, exist_ok=True)
    return passwords_dir


@standup
def pool_signing_key(passwords_dir):
    return passwords_dir / "POOL"


@standup
def password_file(test_dir):
    return test_dir / "condor" / "password"


@standup
def offline_password_file(test_dir):
    return test_dir / "condor" / "password-offline"


@standup
def wrong_password_file(test_dir):
    wrong_password_file = open(test_dir / "condor" / "wrong-password", "w")
    wrong_password_file.write("wrong password file\n")
    wrong_password_file.close()
    os.chmod(test_dir / "condor" / "wrong-password", 0o600)
    yield test_dir / "condor" / "wrong-password"


@standup
def condor(test_dir, passwords_dir, system_tokens_dir, pool_signing_key, password_file, user_tokens_dir):
    with Condor(
        local_dir=test_dir / "condor",
        clean_local_dir_before=False,
        config={
            "DAEMON_LIST" : "MASTER COLLECTOR",
            "MASTER_DEBUG" : "D_SECURITY",
            "TOOL_DEBUG" : "D_SECURITY",
            "SHARED_PORT_PORT" : "0",

            "LOCAL_CONFIG_DIR" : "$(LOCAL_DIR)/config",

            "SEC_DEFAULT_AUTHENTICATION" : "REQUIRED",
            "SEC_CLIENT_AUTHENTICATION" : "REQUIRED",
            # we will enable this config statement *after* condor starts up
            #"SEC_DEFAULT_AUTHENTICATION_METHODS" : "TOKEN",

            "SEC_PASSWORD_DIRECTORY" : passwords_dir,
            "SEC_TOKEN_SYSTEM_DIRECTORY" : system_tokens_dir,
            "SEC_TOKEN_POOL_SIGNING_KEY_FILE" : pool_signing_key,
            "TOOL.SEC_TOKEN_POOL_SIGNING_KEY_FILE" : password_file,
            "SEC_TOKEN_DIRECTORY" : user_tokens_dir,

            # FIXME: I want there to be no permissions in the security system
            # other than condor_pool@*/* and administrator@domain/*.  Get ZKM
            # to review/test these settings for that purpose.
            "ALLOW_ADMINISTRATOR" : "condor_pool@*/*, administrator@domain/*",
            "ALLOW_OWNER" : "condor_pool@*/*, administrator@domain/*",
            "ALLOW_CONFIG" : "condor_pool@*/*, administrator@domain/*",
            "DENY_ALL" : "*",
        }
    ) as condor:
        yield condor


# create a local config file that disables all auth methods other than TOKEN
@action
def token_config_file(condor, config_dir):
    token_config_file = open(config_dir / "00token-config", "w")
    token_config_file.write("SEC_DEFAULT_AUTHENTICATION_METHODS = TOKEN\n")
    token_config_file.close()
    os.chmod(config_dir / "00token-config", 0o600)
    yield config_dir / "00token-config"


# reconfig the daemons so they pick up the changed config and the generated POOL key
# reconfig is a bit async, so we sleep 5 to give it time to take effect
@action
def reconfigure_daemons(condor, token_config_file):
    condor.run_command(["condor_reconfig", "-all"], timeout=20)
    time.sleep(5)


# copy the POOL key to the filename that tools use
@action
def copy_pool_key(condor, reconfigure_daemons, pool_signing_key, password_file):
    shutil.copyfile(pool_signing_key, password_file)
    os.chmod(password_file, 0o600)


class TestAuthProtocolToken:

    def test_if_pool_signing_key_generated(self, condor, pool_signing_key):
        assert os.path.isfile(pool_signing_key)

    def test_generated_token_signing_key(self, condor, copy_pool_key):
        rval = condor.run_command(["condor_ping", "-type", "collector", "-table", "ALL", "-debug"], timeout=20)
        assert rval.returncode == 0




