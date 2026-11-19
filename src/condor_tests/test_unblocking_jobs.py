#!/usr/bin/env pytest

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

import pytest

from ornithology import (
    Condor,
    action,
    ClusterState,
    JobStatus,
    JobQueue,
)

import os
import re
import sys
import time
import htcondor2


@action
def the_startd_condor(test_dir):
    local_dir = test_dir / "startd_condor.d"
    with Condor(
        local_dir=local_dir,
        config={
            "NUM_CPUS":                         5,
            "KILLING_TIMEOUT":                  1,
            "JOB_DEFAULT_LEASE_DURATION":       1,
            "SCHEDD_JOB_QUEUE_LOG_FLUSH_DELAY": 1,
        },
    ) as the_condor:
        yield the_condor


@action
def the_shadow_condor(test_dir):
    local_dir = test_dir / "shadow_condor.d"
    with Condor(
        local_dir=local_dir,
        config={
            "NUM_CPUS":                     5,
            "KILLING_TIMEOUT":              1,
            "JOB_DEFAULT_LEASE_DURATION":   1,
            "SHADOW_DEBUG":                 "D_PID D_CATEGORY D_SUB_SECOND D_FULLDEBUG",
        },
    ) as the_condor:
        yield the_condor


@action
def the_schedd_condor(test_dir):
    local_dir = test_dir / "schedd_condor.d"
    with Condor(
        local_dir=local_dir,
        config={
            "NUM_CPUS":                     5,
            "KILLING_TIMEOUT":              1,
            "JOB_DEFAULT_LEASE_DURATION":   1,
        },
    ) as the_condor:
        yield the_condor


def blocked_jobs_in_queue(a_condor):
    job_description = {
        "universe":                 "vanilla",
        "shell":                    "sleep 5",
        "should_transfer_files":    "YES",
        "request_cpus":             1,
        "request_memory":           1024,
        "log":                      "the_job.log",
        "LeaveInQueue":             True,
        # This is the magic.
        "MY.CommonInputFiles":      '"debug://sleep/3000"',
    }

    job_handle = a_condor.submit(
       description=job_description,
       count=5,
    )

    # All seven jobs should be BLOCKED for 300 seconds because of the
    # debug plug-in's sleep, giving us a wide window to detect when we
    # can shut the startd down.

    schedd = a_condor.get_local_schedd()

    deadline = time.time() + 300
    while True:
        time.sleep(5)

        # Unfortunately, Ornithology's ClusterState doesn't know about
        # BLOCKED yet, so we have to ask the schedd.
        results = schedd.query(
            constraint=f'ClusterID == {job_handle.clusterid}',
            projection=["ProcID", "JobStatus"],
        )

        # We only need one, but we do expect all jobs to be BLOCKED.
        statuses = [result.get('JobStatus', -1) == 9 for result in results]
        if all(statuses):
            print(f"\n\nAll jobs in cluster {job_handle.clusterid} are blocked.")
            break

        assert time.time() < deadline;

    # Turn off the negotiator.  (So that the schedd can't try to re-run
    # jobs after transfer shadow dies but before the startd invalidates.)
    a_condor.run_command(
        ['condor_off', '-negotiator'],
        timeout=10,
        echo=False,
    )

    # Wait for it to die.
    cp = a_condor.run_command(
        ['condor_who', '-quick', '-wait:30', 'NEGOTIATOR == "Hold"'],
        timeout=30,
        echo=False,
    )

    return job_handle


@action
def the_startd_jobs(the_startd_condor):
    job_handle = blocked_jobs_in_queue(the_startd_condor)

    # Turn of the startd.  This will take out the transfer shadow.
    the_startd_condor.run_command(
        ['condor_off', '-startd'],
        timeout=10,
        echo=False,
    )

    # Wait for it to die.
    cp = the_startd_condor.run_command(
        ['condor_who', '-quick', '-wait:30', 'STARTD == "Hold"'],
        timeout=30,
        echo=False,
    )

    return job_handle


@action
def the_shadow_jobs(the_shadow_condor):
    job_handle = blocked_jobs_in_queue(the_shadow_condor)

    deadline = time.time() + 30
    while time.time() < deadline:
        # Grovel the shadow log to find the transfer shadow's PID.
        shadow_log = the_shadow_condor.shadow_log.open()
        parsed_lines = list(shadow_log.read())
        # The `lines` property reflects only previous read() calls,
        # which is rather annoying when you need the raw log entries.
        raw_lines = shadow_log.lines

        jobID = f"{job_handle.clusterid}.-1"
        pattern = re.compile('\\(pid:(\\d+)\\)')
        for line in raw_lines:
            if not jobID in line:
                continue
            print(f"Extracing PID from log line: '{line}'.")
            matches = re.search(pattern, line)
            if matches:
                pid = int(matches[1])

                # kill -9 the transfer shadow
                os.kill( pid, 9 )

                return job_handle

        time.sleep(5)

    assert time.time() < deadline


@action
def the_schedd_jobs(the_schedd_condor):
    job_handle = blocked_jobs_in_queue(the_schedd_condor)

    cp = the_schedd_condor.run_command(
        ['condor_who', '-daemons'],
        timeout=30,
        echo=False
    )

    # Find the schedd's PID.
    pid = -1
    for line in cp.stdout.split("\n"):
        if 'Schedd' in line:
            pid = int(line.split()[2])
    assert pid != -1

    # Verify on-disk queue state.
    deadline = time.time() + 30
    while time.time() < deadline:
        blocked_jobs = 0
        with the_schedd_condor.job_queue_log.open(mode="r") as jql:
            for line in jql.readlines():
                if line.startswith("103 ") and " JobStatus " in line:
                    (_, jobid, __, status) = line.split()
                    if jobid.startswith(f"{job_handle.clusterid}."):
                        if status == "9":
                            blocked_jobs += 1
        if blocked_jobs == 5:
            break
        time.sleep(5)
    assert time.time() < deadline
    assert blocked_jobs == 5

    # Don't let the master restart the schedd before we're ready.
    cp = the_schedd_condor.run_command(
        ['condor_off', '-schedd'],
        timeout=30,
        echo=False,
    )

    # Make sure the schedd died.
    cp = the_schedd_condor.run_command(
        ['condor_who', '-quick', '-wait:30', 'SCHEDD == "Hold"'],
        timeout=30,
        echo=False,
    )

    # Turn the schedd back on.
    cp = the_schedd_condor.run_command(
        ['condor_on', '-schedd'],
        timeout=30,
        echo=False,
    )

    # Wait for it to live.
    cp = the_schedd_condor.run_command(
        ['condor_who', '-quick', '-wait:30', 'SCHEDD == "Alive"'],
        timeout=30,
        echo=False,
    )

    return job_handle


def assert_all_jobs_not_blocked(the_condor, the_jobs):
    schedd = the_condor.get_local_schedd()

    deadline = time.time() + 30
    while True:
        time.sleep(5)

        # Unfortunately, Ornithology's ClusterState doesn't know about
        # BLOCKED yet, so we have to ask the schedd.
        results = schedd.query(
            constraint=f'ClusterID == {the_jobs.clusterid}',
            projection=["ProcID", "JobStatus"],
        )
        print(results)

        # None of the jobs may be blocked.
        statuses = [result.get('JobStatus', 9) != 9 for result in results]
        if all(statuses):
            break

        assert time.time() < deadline;

    assert all(statuses)
    assert len(statuses) == 5


class TestBlockedJobs:


    #
    # This test just asserts that turning off the startd results in no
    # blocked jobs; it turns out that's a (very) different case than the
    # transfer shadow dying, and one that we didn't handle properly.
    #
    def test_off_dash_startd(self, the_startd_condor, the_startd_jobs):
        assert_all_jobs_not_blocked(the_startd_condor, the_startd_jobs)


    #
    # This test verifies that no jobs remained blocked after we kill -9 the
    # transfer shadow they're waiting for.
    #
    @pytest.mark.skipif( sys.platform != 'linux', reason="needs kill -9" )
    def test_kill_transfer_shadow(self, the_shadow_condor, the_shadow_jobs):
        assert_all_jobs_not_blocked(the_shadow_condor, the_shadow_jobs)


    #
    # This test verifies that no jobs remained blocked after we kill -9 the
    # the schedd (and the restart it).
    #
    @pytest.mark.skipif( sys.platform != 'linux', reason="needs kill -9" )
    def test_kill_schedd(self, the_schedd_condor, the_schedd_jobs):
        schedd = the_schedd_condor.get_local_schedd()

        # Helpfully, the security subsystem holds onto our session from
        # the previous incarnation of the schedd; but for some reason, it
        # doesn't just retry if the save session has become invalid.
        try:
            results = schedd.query(
                constraint=f'ClusterID == {the_schedd_jobs.clusterid}',
                projection=["ProcID", "JobStatus"],
            )
        except htcondor2.HTCondorException:
            pass

        assert_all_jobs_not_blocked(the_schedd_condor, the_schedd_jobs)
