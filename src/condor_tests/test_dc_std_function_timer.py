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
            "DAEMON_LIST":              "TEST_STDF_TIMER_D COLLECTOR",
            "TEST_STDF_TIMER_D":        "$(LIBEXEC)/test_stdf_timer_d",
            # Ornithology needs this, for some reason.
            "TEST_STDF_TIMER_D_LOG":    f"$(LOG)/TestStdFTimerDLog",
            "DC_DAEMON_LIST":           "+ TEST_STDF_TIMER_D",
        },
    ) as condor:
        yield condor


@action
def the_daemon_log(the_condor):
    with the_condor.use_config():
        libexec = htcondor2.param['LIBEXEC']
    for i in range(1,10):
        the_log = the_condor._get_daemon_log('TEST_STDF_TIMER_D')
        for line in the_log.open().read():
            if "EXITING WITH STATUS" in line.message:
                return the_log
        time.sleep(12)
    assert False


class TestDCStdFunction:

    def test_timer(self, the_condor, the_daemon_log):
        # I'm not at all sure I can just re-open the daemon log.
        the_log = the_condor._get_daemon_log('TEST_STDF_TIMER_D')

        for line in the_log.open().read():
            if "std::function timer fired." == line.message:
                return
            if "c function timer fired." == line.message:
                assert False
        assert False

