#!/usr/bin/env pytest

# this test replicates the first part of job_late_materialize_py

import logging

import time
import sys

import htcondor2 as htcondor

from ornithology import (
    config,
    standup,
    action,
    Condor,
    write_file,
    parse_submit_result,
    JobID,
    SetAttribute,
    SetJobStatus,
    JobStatus,
    in_order,
    SCRIPTS,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@standup
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "NUM_CPUS": "10",
            "NUM_SLOTS": "10",  # must be larger than the max number of jobs we hope to materialize
            "SCHEDD_MATERIALIZE_LOG": "$(LOG)/MaterializeLog",
            "SCHEDD_DEBUG": "D_MATERIALIZE:2 D_CAT $(SCHEDD_DEBUG)",
        },
    ) as condor:
        yield condor


MAX_IDLE = {"idle=2": 2, "idle=3": 3}
MAX_MATERIALIZE = {"materialize=2": 2, "materialize=3": 3}


@action(params=MAX_IDLE)
def max_idle(request):
    return request.param


@action(params=MAX_MATERIALIZE)
def max_materialize(request):
    return request.param


@action
def jobids_for_sleep_jobs(test_dir, path_to_sleep, condor, max_idle, max_materialize):
    sub_description = """
        executable = {exe}
        arguments = 1

        request_memory = 1MB
        request_disk = 1MB

        max_materialize = {max_materialize}
        max_idle = {max_idle}

        queue {q}
    """.format(
        exe=path_to_sleep,
        max_materialize=max_materialize,
        max_idle=max_idle,
        q=max_materialize + max_idle + 1,
    )
    submit_file = write_file(test_dir / "queue.sub", sub_description)


    # submit_cmd = condor.run_command(["condor_submit", submit_file])
    # clusterid, num_procs = parse_submit_result(submit_cmd)

    submit = htcondor.Submit(sub_description)
    schedd = condor.get_local_schedd()
    result = schedd.submit(submit)
    # print(result, file=sys.stderr)
    clusterid = result.cluster()
    num_procs = result.num_procs()
    # print(f"num_procs {num_procs}\n", file=sys.stderr)

    jobids = [JobID(clusterid, n) for n in range(num_procs)]

    condor.job_queue.wait_for_events(
        {jobid: [SetJobStatus(JobStatus.COMPLETED)] for jobid in jobids}
    )


    return jobids


@action
def num_materialized_jobs_history(condor, jobids_for_sleep_jobs):
    num_materialized = 0
    history = []
    for jobid, event in condor.job_queue.filter(
        lambda j, e: j in jobids_for_sleep_jobs
    ):
        if event == SetJobStatus(JobStatus.IDLE):
            num_materialized += 1
        if event == SetJobStatus(JobStatus.COMPLETED):
            num_materialized -= 1

        history.append(num_materialized)

    return history


@action
def num_idle_jobs_history(condor, jobids_for_sleep_jobs):
    num_idle = 0
    history = []
    for jobid, event in condor.job_queue.filter(
        lambda j, e: j in jobids_for_sleep_jobs
    ):
        if event == SetJobStatus(JobStatus.IDLE):
            num_idle += 1
        if event == SetJobStatus(JobStatus.RUNNING):
            num_idle -= 1

        history.append(num_idle)

    return history


class TestLateMaterializationLimits:
    def test_all_jobs_ran(self, condor, jobids_for_sleep_jobs):
        for jobid in jobids_for_sleep_jobs:
            assert in_order(
                condor.job_queue.by_jobid[jobid],
                [
                    SetJobStatus(JobStatus.IDLE),
                    SetJobStatus(JobStatus.RUNNING),
                    SetJobStatus(JobStatus.COMPLETED),
                ],
            )

    def test_never_more_materialized_than_max(
        self, num_materialized_jobs_history, max_materialize
    ):
        assert max(num_materialized_jobs_history) <= max_materialize

    def test_hit_max_materialize_limit(
        self, num_materialized_jobs_history, max_materialize
    ):
        assert max_materialize in num_materialized_jobs_history

    def test_never_more_idle_than_max(
        self, num_idle_jobs_history, max_idle, max_materialize
    ):
        assert max(num_idle_jobs_history) <= min(max_idle, max_materialize)

    def test_hit_max_idle_limit(self, num_idle_jobs_history, max_idle, max_materialize):
        assert min(max_idle, max_materialize) in num_idle_jobs_history


@action
def clusterid_for_itemdata(test_dir, path_to_sleep, condor):
    # enable late materialization, but with a high enough limit that they all
    # show up immediately (on hold, because we don't need to actually run
    # the jobs to do the tests)
    sub_description = """
        executable = {exe}
        arguments = 0

        request_memory = 1MB
        request_disk = 1MB

        max_materialize = 5

        hold = true

        My.Foo = "$(Item)"

        queue in (A, B, C, D, E)
    """.format(
        exe=path_to_sleep,
    )
    submit_file = write_file(test_dir / "queue_in.sub", sub_description)


    # submit_cmd = condor.run_command(["condor_submit", submit_file])
    # clusterid, num_procs = parse_submit_result(submit_cmd)

    submit = htcondor.Submit(sub_description)
    schedd = condor.get_local_schedd()
    result = schedd.submit(submit)
    clusterid = result.cluster()
    num_procs = result.num_procs()


    jobids = [JobID(clusterid, n) for n in range(num_procs)]

    condor.job_queue.wait_for_events(
        {jobid: [SetAttribute("Foo", None)] for jobid in jobids}
    )

    yield clusterid

    condor.run_command(["condor_rm", clusterid])


class TestLateMaterializationItemdata:
    def test_itemdata_turns_into_job_attributes(self, condor, clusterid_for_itemdata):
        actual = {}
        for jobid, event in condor.job_queue.filter(
            lambda j, e: j.cluster == clusterid_for_itemdata
        ):
            # the My. doesn't end up being part of the key in the jobad
            if event.matches(SetAttribute("Foo", None)):
                actual[jobid] = event.value

        expected = {
            # first item gets put on the clusterad!
            JobID(clusterid_for_itemdata, -1): '"A"',
            JobID(clusterid_for_itemdata, 1): '"B"',
            JobID(clusterid_for_itemdata, 2): '"C"',
            JobID(clusterid_for_itemdata, 3): '"D"',
            JobID(clusterid_for_itemdata, 4): '"E"',
        }

        assert actual == expected

    def test_query_produces_expected_results(self, condor, clusterid_for_itemdata):
        with condor.use_config():
            schedd = htcondor.Schedd()
            ads = schedd.query(
                constraint="clusterid == {}".format(clusterid_for_itemdata),
                # the My. doesn't end up being part of the key in the jobad
                projection=["clusterid", "procid", "foo"],
            )

        actual = [ad["foo"] for ad in sorted(ads, key=lambda ad: int(ad["procid"]))]
        expected = list("ABCDE")

        assert actual == expected
