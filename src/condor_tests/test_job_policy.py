#!/usr/bin/env pytest

#
# Does HOLD_IF_MEMORY_EXCEEDED work during normal (non-draining) operation?
#

import logging
import textwrap
from pathlib import Path

from ornithology import (
    config,
    standup,
    action,
    JobStatus,
    ClusterState,
    Condor,
)


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


# This layer of indirection may not be necessary if we're not parameterizing.
@config
def condor_config():
    config = {
        "STARTD_DEBUG": "D_SUB_SECOND D_FULLDEBUG D_JOB",
        "MAXJOBRETIREMENTTIME": 300,
    }
    raw_config = textwrap.dedent(
        """
        use feature : PartitionableSlot
        use policy : HOLD_IF_MEMORY_EXCEEDED
    """
    )
    return {"config": config, "raw_config": raw_config}


@standup
def condor(condor_config, test_dir):
    with Condor(test_dir / "condor", **condor_config) as condor:
        yield condor


@standup
def job_file(test_dir):
    job_file = test_dir / "condition_sleep.py"
    contents = textwrap.dedent(
        """
        import os
        import sys
        import time
        import signal

        signal.signal( signal.SIGTERM, signal.SIG_IGN )
        killFile = sys.argv[1]
        maxSleep = sys.argv[2]

        for i in range( 0, int(maxSleep) ):
            if os.path.isfile( killFile ):
                memoryWaste = list( range( 0, 1024 * 1024 * 16 ) )
            time.sleep( 1 )

        sys.exit( 0 )
        """
    )
    with open(str(job_file), "w") as f:
        f.write(contents)
    return job_file


@action
def kill_file(test_dir):
    return test_dir / "killfile"


@action
def job_parameters(job_file, kill_file):
    return {
        "executable": "/usr/bin/env",
        "transfer_executable": "false",
        "should_transfer_files": "true",
        "transfer_input_files": str(job_file),
        "universe": "vanilla",
        "arguments": "python {} {}.$(CLUSTER).$(PROCESS) 3600".format(
            str(job_file), kill_file
        ),
        "request_memory": 1,
        "log": "test_job_policy.log",
    }


@action
def job(condor, job_parameters):
    job = condor.submit(job_parameters, count=1)

    yield job

    job.remove()


def ask_to_misbehave(job, proc, kill_file):
    Path("{}.{}.{}".format(kill_file, job.clusterid, proc)).touch()


@action
def job_after_signal(job, kill_file):
    assert job.wait(
        condition=ClusterState.all_running,
        timeout=60,
        fail_condition=ClusterState.any_terminal,
    )

    ask_to_misbehave(job, 0, kill_file)
    return job


class TestHoldIfMemoryExceeded:
    def test_job_is_held(self, job_after_signal):
        assert job_after_signal.wait(
            condition=ClusterState.all_held,
            timeout=60,
            fail_condition=lambda self: self.any_status(
                JobStatus.COMPLETED, JobStatus.REMOVED
            ),
        )
