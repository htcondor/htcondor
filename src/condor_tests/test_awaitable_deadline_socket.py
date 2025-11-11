#!/usr/bin/env pytest

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

import time
from ornithology import *

import htcondor2


@action
def the_condor(test_dir):
    local_dir = test_dir / "condor"

    with Condor(
        local_dir=local_dir,
        config={
            "DAEMON_LIST":
                "TEST_AWAITABLE_DEADLINE_SOCKETD COLLECTOR",
            "TEST_AWAITABLE_DEADLINE_SOCKETD":
                "$(LIBEXEC)/test_awaitable_deadline_socketd",
            # Ornithology needs this, for some reason.
            "TEST_AWAITABLE_DEADLINE_SOCKETD_LOG":
                f"$(LOG)/AwaitableDeadlineSocketLog",
            "DC_DAEMON_LIST":
                "+ TEST_AWAITABLE_DEADLINE_SOCKETD",
        },
    ) as condor:
        yield condor


@action
def the_hot_log(the_condor):
    with the_condor.use_config():
        libexec = htcondor2.param['LIBEXEC']
    the_log = the_condor._get_daemon_log('TEST_AWAITABLE_DEADLINE_SOCKETD')
    for line in the_log.open().read():
        if line.message.startswith('DaemonCore: command socket at '):
            [prefix, sinful] = line.message.split('DaemonCore: command socket at ')
            result = the_condor.run_command(
                [f'{libexec}/test_awaitable_deadline_socket_client', '-hot', sinful]
            )
            assert result.returncode == 0
            return the_log
    assert False


@action
def the_timeout_log(the_condor):
    with the_condor.use_config():
        libexec = htcondor2.param['LIBEXEC']
    the_log = the_condor._get_daemon_log('TEST_AWAITABLE_DEADLINE_SOCKETD')
    for line in the_log.open().read():
        if line.message.startswith('DaemonCore: command socket at '):
            [prefix, sinful] = line.message.split('DaemonCore: command socket at ')
            result = the_condor.run_command(
                [f'{libexec}/test_awaitable_deadline_socket_client', '-timeout', sinful]
            )
            assert result.returncode == 0
            return the_log
    assert False


class TestDCStdFunction:

    def test_hot_socket(self, the_condor, the_hot_log):
        # I'm not at all sure I can just re-open the_hot_log.
        the_log = the_condor._get_daemon_log('TEST_AWAITABLE_DEADLINE_SOCKETD')
        for line in the_log.open().read():
            print(line)
            if line.message == "[hot] Exchange complete.":
                return
        assert False


    def test_timeout(self, the_condor, the_timeout_log):
        # I'm not at all sure I can just re-open the_timeout_log.
        the_log = the_condor._get_daemon_log('TEST_AWAITABLE_DEADLINE_SOCKETD')
        for line in the_log.open().read():
            print(line)
            if line.message == "[timeout] Timed out waiting for third string.":
                return
        assert False
