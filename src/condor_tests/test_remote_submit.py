#!/usr/bin/env pytest

import logging
import os
from time import sleep
from pathlib import Path
import htcondor2 as htcondor

from ornithology import (
    config,
    standup,
    action,
    Condor,
    ClusterState,
    ClusterHandle,
    JobStatus
)


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

@standup
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor",
        raw_config="""
            SCHEDD_INTERVAL = 5
            PERIODIC_EXPR_INTERVAL = 5
        """,
    ) as condor:
        yield condor

@action
def remote_job(condor, path_to_sleep):
    schedd = condor.get_local_schedd()
    jdl = htcondor.Submit(
        f"""
        executable = {path_to_sleep}
        arguments = 0
        log = the_job.log
        """
        )
    sr = schedd.submit(jdl, spool=True)
    schedd.spool(sr)

    # This is a hack, although it duplicates what the_condor.submit() would
    # do if that function supported spooling.
    ch = ClusterHandle(condor, sr)
    # This is a really egregious hack because the EventLog class (and company)
    # don't understand that a log file with a relative path may mean "in SPOOL".
    ch.event_log._event_log_path = Path(condor.spool_dir / f"{sr.cluster()}" / "0" / f"cluster{sr.cluster()}.proc0.subproc0" / "the_job.log")

    assert ch.wait(condition=ClusterState.all_complete, timeout=60)

    # The event log shows the completion event before the schedd's shadow
    # reaper has necessarily updated JobStatus to COMPLETED.  retrieve()
    # only sets StageOutStart/StageOutFinish for jobs already in a terminal
    # state, so we must wait for the schedd to catch up.
    for _ in range(60):
        ads = schedd.query(
            constraint=f"ClusterID == {sr.cluster()}",
            projection=["JobStatus"],
        )
        if ads and ads[0]["JobStatus"] == JobStatus.COMPLETED:
            break
        sleep(1)

    schedd.retrieve(sr.cluster())

    return ch

class TestRemoteSubmit:
    def test_remote_submit_retirement(self, condor, remote_job):
        schedd = condor.get_local_schedd()
        # Poke the schedd to retire the job more quickly
        schedd.reschedule()

        in_queue = True
        i = 0
        while i < 60 and in_queue:
            ads = schedd.query()
            in_queue = len(ads) > 0
            i += 1
            sleep(1)

        assert not in_queue
