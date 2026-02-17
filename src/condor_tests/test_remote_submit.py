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
        """,
    ) as condor:
        yield condor

@action
def remote_job(condor):
    schedd = condor.get_local_schedd()
    jdl = htcondor.Submit(
        """
        executable = /bin/sleep
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
    schedd.retrieve(sr.cluster())

    return ch

class TestRemoteSubmit:
    def test_remote_submit_retirement(self, condor, remote_job):
        assert remote_job.wait(condition=ClusterState.all_complete, timeout=60)
        assert remote_job.state[0] == JobStatus.COMPLETED
        schedd = condor.get_local_schedd()
        schedd.retrieve(remote_job.clusterid)
        schedd.reschedule()

        in_queue = True
        i = 0
        while i < 60 and in_queue:
            ads = schedd.query()
            in_queue = len(ads) > 0
            i += 1
            sleep(1)

        assert not in_queue
