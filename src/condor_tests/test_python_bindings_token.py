#!/usr/bin/env pytest

# Test explicit use of an in-memory token in the bindings

import htcondor2 as htcondor
import time
import logging

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@standup
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "SEC_DEFAULT_AUTHENTICATION_METHODS": "TOKEN,FS",
            "DAEMON_LIST": "COLLECTOR MASTER",
            "ALLOW_WRITE": "*",
            "ENABLE_IPV6": "false",
            "COLLECTOR_DEBUG": "D_SECURITY:2",
        }
    ) as condor:
        yield condor

@action
def bespoke_token(condor):
    rv = condor.run_command(
        ["condor_token_create", "-identity", "vulture@foo"],
        timeout=8,
    )
    assert rv.returncode == 0
    return rv.stdout


class TestPythingBindingsToken:

    def test_ping_with_token(self, condor, bespoke_token):
        # We need condor.use_config() so that the locate() of a local daemon
        # looks in the correct LOG directory for the daemon address file.
        with condor.use_config():
            collector = condor.get_local_collector()
            collector_ad = None
            collector_ad = collector.locate(htcondor.DaemonType.Collector)

        ctxt = htcondor.SecurityContext(token=bespoke_token)
        ping_ad = htcondor.ping(collector_ad, "WRITE", ctxt)

        assert ping_ad["Authentication"] == "YES"
        assert ping_ad["AuthMethods"] == "IDTOKENS"
        assert ping_ad["MyRemoteUserName"] == "vulture@foo"
