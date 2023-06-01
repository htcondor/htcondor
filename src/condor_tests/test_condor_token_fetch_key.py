#!/usr/bin/env pytest

#
# Test to make sure that `condor_token_fetch -key` actually works.
#
# This means returning a token signed by the specified key, and no
# other key.
#

import json
import logging

from ornithology import (
    config,
    action,
    Condor,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@action
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "SEC_TOKEN_FETCH_ALLOWED_SIGNING_KEYS": "POOL, test_key",
        },
    ) as condor:
        test_key = condor.passwords_dir / "test_key"
        test_key.write_bytes(b'test_key_value')
        test_key.chmod(0o600)

        yield condor


def token_to_dict(test_dir, condor, token):
    token_file = test_dir / "token.tmp"
    token_file.write_text(token)
    rv = condor.run_command(
        ["condor_token_list", "-dir", test_dir],
        timeout=8,
    )
    assert rv.returncode == 0

    d = {}
    s = rv.stdout.split(" ")
    d["Header"] = json.loads(s[1])
    d["Payload"] = json.loads(s[3])

    # The 'iat' field probably differs; the 'jti' field must.  Since
    # it is erroneous for these fields ("claims" in JWT parlance) to
    # not exist, erroring out on a KeyError is appropriate here.
    del d["Payload"]["iat"]
    del d["Payload"]["jti"]

    return d


@action
def default_token(test_dir, condor):
    rv = condor.run_command(
        ["condor_token_fetch"],
        timeout=8,
    )
    assert rv.returncode == 0
    return token_to_dict(test_dir, condor, rv.stdout)


@action
def pool_token(test_dir, condor):
    rv = condor.run_command(
        ["condor_token_fetch", "-key", "POOL"],
        timeout=8,
    )
    assert rv.returncode == 0
    return token_to_dict(test_dir, condor, rv.stdout)


@action
def test_token(test_dir, condor):
    rv = condor.run_command(
        ["condor_token_fetch", "-key", "test_key"],
        timeout=8,
    )
    assert rv.returncode == 0
    return token_to_dict(test_dir, condor, rv.stdout)


class TestCondorTokenFetchKey:
    def test_default_equals_pool(self, default_token, pool_token):
        assert default_token == pool_token

    def test_default_not_test(self, default_token, test_token):
        assert default_token != test_token
