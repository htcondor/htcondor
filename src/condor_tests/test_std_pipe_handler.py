#!/usr/bin/env pytest

from ornithology import (
    action,
    Condor
)

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

import time


@action
def the_condor(test_dir):
    local_dir = test_dir / "condor"

    with Condor(
        local_dir=local_dir,
        config={
            "TPIPEH":       "$(LIBEXEC)/test_std_pipe_handlerd",
            "DAEMON_LIST":  "$(DAEMON_LIST) TPIPEH",
            # Ornithology needs this, for some reason.
            "TPIPEH_LOG":    f"$(LOG)/TestStdPipeHandlerDLog",
        },
    ) as condor:
        yield condor


@action
def the_daemon_log(the_condor):
    the_log = the_condor._get_daemon_log('TPIPEH')
    the_open_log = the_log.open()

    for timeout in range(1, 30):
        for line in the_open_log.read():
            print( f"  >>> {line}" )
            if "EXITING WITH STATUS" in line.message:
                return the_log
        time.sleep(1)
    assert False


class TestStdPipeHandler:

    def test_std_pipe_handler(self, the_condor, the_daemon_log):
        # I'm not at all sure I can just re-open the daemon log.
        the_log = the_condor._get_daemon_log('TPIPEH')

        found_complete_read = False
        for line in the_log.open().read():
            print( f"  >>> {line}" )
            if not found_complete_read:
                if "Read 0x7 0x4 0x3 0x71 0x22 0x70 0x61 0x55 0x24 0xaa 0xbc 0xad 0xff 0xee 0xcb 0x7 0x4 0x3 0x71 0x22 so far." in line.message:
                    found_complete_read = True
                continue
            else:
                if "Done!" in line.message:
                    return
                if "Read " in line.message and "so far." in line.message:
                    assert False
        assert False
