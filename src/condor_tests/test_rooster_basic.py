#!/usr/bin/env pytest

"""
Basic test for Rooster with 100 static slots.

This test sets up a personal HTCondor with:
- 100 static slots on the startd
- Each slot has RANK = 1
- Rooster daemon running
"""

import logging
import time

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@standup
def condor(test_dir):
    """
    Set up a personal HTCondor with 100 static slots and Rooster.
    """
    with Condor(
        local_dir=test_dir / "condor",
        config={
            # Enable Rooster in the daemon list
            "DAEMON_LIST": "MASTER COLLECTOR NEGOTIATOR SCHEDD STARTD ROOSTER",

            # Configure 100 static slots
            "NUM_CPUS": "100",
            "NUM_SLOTS_TYPE_1": "100",
            "MEMORY" : "102400",
            "DISK" : "1024000",
            "SLOT_TYPE_1": "cpus=1",
            "SLOT_TYPE_1_PARTITIONABLE": "FALSE",

            # Set RANK = 1 for all slots
            "RANK": "1",

            # Rooster configuration
            "ROOSTER_INTERVAL": "6",  # Poll every 6 seconds
            "ROOSTER_UNHIBERNATE": "TRUE",  # fake hibernate everyone
            "ROOSTER_MAX_UNHIBERNATE": "10",
            "ROOSTER_WAKEUP_CMD": '''"/bin/true"''',  # Dummy command

            # Debug logging
            "ROOSTER_DEBUG": "D_FULLDEBUG",
            "STARTD_DEBUG": "D_FULLDEBUG",

            # Disable shared port for simplicity
            #"USE_SHARED_PORT": "FALSE",
        }
    ) as condor:
        yield condor


@action
def rooster_running(condor):
    """Verify the Rooster daemon is running."""

    # Check if Rooster log exists
    rooster_log = condor.local_dir / "log" / "RoosterLog"
    assert rooster_log.exists(), "RoosterLog should exist"

    # Read log to verify Rooster started
    with open(rooster_log, 'r') as f:
        log_content = f.read()
        assert len(log_content) > 0, "RoosterLog should not be empty"
        logger.info(f"Rooster log size: {len(log_content)} bytes")

    return True

class TestRoosterBasic:
    """Basic tests for Rooster with 100 static slots."""

    def test_rooster_is_running(self, rooster_running):
        """Verify Rooster daemon started successfully."""
        assert rooster_running
