#!/usr/bin/env pytest

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
# Run a personal condor with FAMILY auth turned off.  Run a CIF job on that
# condor and check the startd log to verify that MATCH auth was used.
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

            # Log the security method used to authenticated command_data_slot.
            "STARTD_DEBUG":             "D_CATEGORY D_PID D_SUB_SECOND D_TEST",
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
def the_running_job( test_dir, the_condor, the_common_file ):
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

    assert job_handle.wait(
        timeout=120,
        condition=ClusterState.running_exactly(1),
        fail_condition=ClusterState.any_terminal,
    )

    return job_handle


class TestCommandDataSlot:

    def test_command_data_slot(self, the_condor, the_running_job):
        startd_log = the_condor.startd_log.open()

        authenticatedViaMatch = False
        authenticatedViaOnlyMatch = True
        for entry in startd_log.read():
            print(entry)
            if 'D_TEST' in entry.tags:
                if entry.message.startswith( "command_data_slot(): authenticated via" ):
                    if entry.message.endswith( " MATCH."):
                        authenticatedViaMatch = True
                    else:
                        authenticatedViaOnlyMatch = False

        assert authenticatedViaMatch and authenticatedViaOnlyMatch
