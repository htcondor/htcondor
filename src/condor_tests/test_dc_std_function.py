#!/usr/bin/env pytest

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

import time
from ornithology import *


@action
def the_attr():
    return f"attr_{int(time.time() % 60)}"


@action
def the_value():
    return f"{int((time.time() % 3600)/60)}"


@action
def the_condor(test_dir, the_attr, the_value):
    local_dir = test_dir / "condor"

    with Condor(
        local_dir=local_dir,
        config={
            "DAEMON_LIST":                  "TEST_DC_STD_FUNCTIOND",
            "TEST_DC_STD_FUNCTIOND":        "$(LIBEXEC)/test_dc_std_functiond",
            "TEST_DC_STD_FUNCTIOND_ARGS":   f"{the_attr} {the_value}",
            # Ornithology needs this, for some reason.
            "TEST_DC_STD_FUNCTIOND_LOG":    f"$(LOG)/TestDcStdFunctionLog",
            "DC_DAEMON_LIST":               "+ TEST_DC_STD_FUNCTIOND",
        },
    ) as condor:
        yield condor


@action
def the_reply(the_condor, the_attr):
    the_log = the_condor._get_daemon_log('TEST_DC_STD_FUNCTIOND')
    for line in the_log.open().read():
        if line.message.startswith('DaemonCore: command socket at '):
            [prefix, sinful] = line.message.split('DaemonCore: command socket at ')
            result = the_condor.run_command(
                ['condor_status', '-af', the_attr, '-direct', sinful]
            )
            assert result.returncode == 0
            return result.stdout
    assert False


class TestDCStdFunction:

    def test_replied_correctly(self, the_reply, the_value):
        assert the_reply == the_value
