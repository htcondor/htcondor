#!/usr/bin/env pytest

import time
import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

from getpass import getuser
from socket import gethostname

from ornithology import (
    Condor,
    action,
    ClusterState,
)

#
# Run a personal condor with FAMILY auth turned off and COMMAND_DATA_SLOT
# requiring ADMINISTRATOR access; deny all such access in the security config.
#
# This test verifies that we delete the transfer shaodw's record if we don't
# to spawn it because COMMAND_DATA_SLOT failed.
#


@action
def the_condor( test_dir ):
    local_dir = test_dir / "the_condor.d"

    user_at_host = f"{getuser()}@{gethostname()}"
    print(user_at_host)

    with Condor(
        local_dir=local_dir,
        config = {
            # Turn of FAMILY session, so we can test (for) MATCH.
            "SEC_USE_FAMILY_SESSION":   "FALSE",

            # Fix up the other auth levels.
            "ALLOW_ADVERTISE_SCHEDD":   user_at_host,
            "ALLOW_ADVERTISE_STARTD":   user_at_host,
            "ALLOW_DAEMON":             user_at_host,
            "ALLOW_NEGOTIATOR":         user_at_host,

            "STARTD_DEBUG":             "D_CATEGORY D_PID D_SUB_SECOND",
            # Log the deletion of the transfer shadow record after the failure
            # to create the corresponding data slot.
            "SCHEDD_DEBUG":             "D_CATEGORY D_PID D_SUB_SECOND D_TEST",

            # Testing knob.  DO NOT DOCUMENT.
            "COMMAND_DATA_SLOT_IS_ADMINISTRATOR":   "TRUE",
            # Nobody can be administrator (and thus COMMAND_DATA_SLOT)
            "DENY_ADMINISTRATOR":                   "*",
            # ... but we want to be able to shut the test down.
            "MASTER.ALLOW_ADMINISTRATOR":           user_at_host,
            "MASTER.DENY_ADMINISTRATOR":            "none",
        },
    ) as the_condor:
        yield the_condor


@action
def the_common_file( test_dir ):
    common_file = test_dir / "the_common_file"
    contents = "1234567890abcdef" * 64 * 128
    common_file.write_text(contents)

    return common_file


@action
def the_submitted_job( test_dir, the_condor, the_common_file ):
    job_description = {
        "universe":                 "vanilla",
        "shell":                    "cat common-file; sleep 60",
        "should_transfer_input":    "YES",
        "request_CPUs":             1,
        "request_memory":           1,
        "request_disk":             1,
        "log":                      test_dir / "the_running_job.log",
        "output":                   test_dir / "the_running_job.out",
        "MY.CommonInputFiles":      f'"{str(the_common_file)}"',
    }

    job_handle = the_condor.submit(
        description=job_description,
        count=1,
    )

    return job_handle


class TestShadowRecCleanup:

    def test_shadowrec_cleanup(self, the_condor, the_submitted_job):
        #
        # The schedd should get a match from the negotiator in the next
        # negotiation cycle, which should prompt an almost immediate attempt
        # to create a data slot, which should almost immediately fail.
        #
        # We could try a polling loop of the schedd (or startd) log to see
        # these events happen, but since that shouldn't change how long
        # we're willing to wait, let's keep things simple for now.
        #
        time.sleep(20)

        #
        # Arguably, we should implement a separate command in the schedd
        # that dumps its table of shadow records and examine that to make
        # sure that (in this case) that it's empty.  This would require
        # some cleverness to make sure that the job wasn't started again
        # (by the next negotiation cycle; if it is, it's a different bug),
        # which we could borrow from test_unblocking_jobs.py, but for now,
        # we'll rely on finding the D_TEST text.
        #
        schedd_log = the_condor.schedd_log.open()

        shadow_record_was_deleted = False
        for entry in schedd_log.read():
            print(entry)

            if entry.message == "Deleting shadow record after failure to create data slot.":
                 shadow_record_was_deleted = True

        assert( shadow_record_was_deleted )
