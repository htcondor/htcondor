#!/usr/bin/env pytest

#
# Every job in the same scope (the DAGMan job ID, if set; otherwise the
# cluster ID) which uses common file transfer must transfer the same common
# files, in the same order.  The first transfer shadow in each scope reports
# a hash of each catalog it staged to the schedd, which records them in the
# scope's cluster ad.  Later transfer shadows check their catalogs against
# those hashes before staging anything, and job shadows check theirs before
# mapping anything; either way, a mismatch puts the job on hold with
# HoldReasonCode 50 [CommonFilesMismatch].  The HoldReasonSubCode is 1 or 2
# if the files differ in a cluster or DAG, and 3 or 4 if only their order
# does.  Paths are canonicalized before hashing.
#
# We test:
#   - cluster_match: two procs share an IWD and a relative common file;
#     both must run.
#   - cluster_mismatch: two procs name the same relative common file but
#     have different IWDs, so they would transfer different files; exactly
#     one must be held (by its job shadow, since the list strings are the
#     same and so it maps the other's staged copy).
#   - dag_mismatch: two clusters in the same DAG name different files;
#     the second must be held (by its transfer shadow).
#   - dag_order: two clusters in the same DAG name the same files in a
#     different order; the second must be held (by its transfer shadow).
#   - dag_same_files: two clusters in the same DAG name the same file via
#     different paths (one with "..", one through a symlinked directory);
#     both must run.
#   - dag_same_dir_contents: two clusters in the same DAG transfer the
#     contents of the same directory ("shared/" and, through a symlink,
#     "link/"); both must run, because the directory's name doesn't land
#     in the sandbox.
#
# We don't run DAGMan, because it (rightly) refuses to submit DAGs whose
# nodes' common files differ; instead, each "DAGMan job" is a held
# placeholder, whose cluster ad stores the expected hashes.
#

import logging
import time

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

from ornithology import (
    Condor,
    action,
    ClusterState,
    JobStatus,
)


COMMON_FILES_MISMATCH = 50
SUBCODE_CLUSTER = 1
SUBCODE_DAG = 2
SUBCODE_CLUSTER_ORDER = 3
SUBCODE_DAG_ORDER = 4


@action
def the_condor(test_dir):
    with Condor(
        local_dir=test_dir / "the_condor.d",
        config={
            "NUM_CPUS":         4,
            "SCHEDD_DEBUG":     "D_CATEGORY D_SUB_SECOND D_PID D_FULLDEBUG",
            "SHADOW_DEBUG":     "D_CATEGORY D_SUB_SECOND D_PID D_FULLDEBUG",
        },
    ) as the_condor:
        yield the_condor


def job_description(test_dir, name, **kwargs):
    description = {
        "shell":                    "cat common.txt",
        "universe":                 "vanilla",
        "should_transfer_files":    "YES",
        "request_cpus":             1,
        "request_memory":           1,
        # The staged catalog's (quantized) size comes out of the prompting
        # job's disk request, so leave some room.
        "request_disk":             "10M",
        "log":                      (test_dir / f"{name}.log").as_posix(),
        "output":                   (test_dir / f"{name}.$(ClusterId).$(ProcId).out").as_posix(),
        "error":                    (test_dir / f"{name}.$(ClusterId).$(ProcId).err").as_posix(),
    }
    description.update(kwargs)
    return description


def make_file(directory, name, contents):
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / name
    path.write_text(contents)
    return path


def submit_fake_dag(the_condor, test_dir, name, nodes):
    """
    Submit a held placeholder "DAGMan job", then one cluster per entry in
    `nodes` (a list of submit description overrides) in its scope, each
    after the previous one has finished or been held, so that each later
    node's transfer shadow finds the first node's hashes already recorded.
    Return the placeholder and node handles.
    """
    placeholder = the_condor.submit(
        description=job_description(test_dir, f"{name}_placeholder", hold=True),
        count=1,
    )

    handles = []
    for i, overrides in enumerate(nodes):
        handle = the_condor.submit(
            description=job_description(test_dir, f"{name}_{i}",
                **{"MY.DAGManJobId": str(placeholder.clusterid)},
                **overrides,
            ),
            count=1,
        )
        handle.wait(condition=ClusterState.all_terminal, timeout=180)
        handles.append(handle)

    return placeholder, handles


def status_of(result):
    return JobStatus(str(result["JobStatus"]))


TERMINAL_STATUSES = {JobStatus.COMPLETED, JobStatus.HELD, JobStatus.REMOVED}


def query_jobs(the_condor, handles, timeout=60):
    # The shadow writes the terminate event (which ends handle.wait()) before
    # the schedd reaps it and sets JobStatus to COMPLETED, so poll the queue
    # until the schedd has caught up.
    constraint = " || ".join(f"ClusterId == {h.clusterid}" for h in handles)
    deadline = time.time() + timeout
    while True:
        results = the_condor.query(
            constraint=constraint,
            projection=["ClusterId", "ProcId", "JobStatus", "HoldReasonCode", "HoldReasonSubCode", "HoldReason"],
        )
        if all(status_of(r) in TERMINAL_STATUSES for r in results):
            break
        if time.time() >= deadline:
            logger.debug("Timed out waiting for the schedd to finish jobs")
            break
        time.sleep(1)
    logger.debug(f"Results: {results}")
    return results


def assert_one_held_one_completed(results, subcode):
    held = [r for r in results if status_of(r) == JobStatus.HELD]
    completed = [r for r in results if status_of(r) == JobStatus.COMPLETED]

    assert len(held) == 1
    assert len(completed) == 1

    h = held[0]
    assert h["HoldReasonCode"] == COMMON_FILES_MISMATCH
    assert h["HoldReasonSubCode"] == subcode
    return h


def assert_second_node_held(results, handles, subcode):
    # The first node always sets the rule, so the second is the one held,
    # by its transfer shadow, before anything is staged for it.
    h = assert_one_held_one_completed(results, subcode)
    assert h["ClusterId"] == handles[1].clusterid
    assert f"job {handles[0].clusterid}.0" in h["HoldReason"]
    return h


@action
def cluster_match(the_condor, test_dir):
    iwd = test_dir / "cluster_match"
    make_file(iwd, "common.txt", "match\n")

    handle = the_condor.submit(
        description=job_description(test_dir, "cluster_match",
            initialdir=iwd.as_posix(),
            transfer_common_input="common.txt",
            leave_in_queue=True,
        ),
        count=2,
    )
    handle.wait(condition=ClusterState.all_terminal, timeout=180)
    yield query_jobs(the_condor, [handle])
    handle.remove()


@action
def cluster_mismatch(the_condor, test_dir):
    base = test_dir / "cluster_mismatch"
    make_file(base / "dir0", "common.txt", "zero\n")
    make_file(base / "dir1", "common.txt", "one\n")

    handle = the_condor.submit(
        description=job_description(test_dir, "cluster_mismatch",
            initialdir=(base / "dir$(ProcId)").as_posix(),
            transfer_common_input="common.txt",
            leave_in_queue=True,
        ),
        count=2,
    )
    handle.wait(condition=ClusterState.all_terminal, timeout=180)
    yield query_jobs(the_condor, [handle])
    handle.remove()


@action
def dag_mismatch(the_condor, test_dir):
    base = test_dir / "dag_mismatch"
    a = make_file(base / "a", "common.txt", "a\n")
    b = make_file(base / "b", "common.txt", "b\n")

    placeholder, handles = submit_fake_dag(the_condor, test_dir, "dag_mismatch", [
        {"initialdir": base.as_posix(), "transfer_common_input": a.as_posix(), "leave_in_queue": True},
        {"initialdir": base.as_posix(), "transfer_common_input": b.as_posix(), "leave_in_queue": True},
    ])
    yield query_jobs(the_condor, handles), handles
    for h in handles + [placeholder]:
        h.remove()


@action
def dag_order(the_condor, test_dir):
    base = test_dir / "dag_order"
    make_file(base, "a.txt", "a\n")
    make_file(base, "b.txt", "b\n")

    placeholder, handles = submit_fake_dag(the_condor, test_dir, "dag_order", [
        {"initialdir": base.as_posix(), "transfer_common_input": "a.txt, b.txt",
         "shell": "cat a.txt b.txt", "leave_in_queue": True},
        {"initialdir": base.as_posix(), "transfer_common_input": "b.txt, a.txt",
         "shell": "cat a.txt b.txt", "leave_in_queue": True},
    ])
    yield query_jobs(the_condor, handles), handles
    for h in handles + [placeholder]:
        h.remove()


@action
def dag_same_files(the_condor, test_dir):
    base = test_dir / "dag_same_files"
    make_file(base / "shared", "common.txt", "shared\n")
    (base / "x").mkdir(parents=True, exist_ok=True)
    link = base / "link"
    if not link.exists():
        link.symlink_to(base / "shared", target_is_directory=True)

    placeholder, handles = submit_fake_dag(the_condor, test_dir, "dag_same_files", [
        {"initialdir": (base / "x").as_posix(), "transfer_common_input": "../shared/common.txt", "leave_in_queue": True},
        {"initialdir": base.as_posix(), "transfer_common_input": "link/common.txt", "leave_in_queue": True},
    ])
    yield query_jobs(the_condor, handles), handles
    for h in handles + [placeholder]:
        h.remove()


@action
def dag_same_dir_contents(the_condor, test_dir):
    base = test_dir / "dag_same_dir_contents"
    make_file(base / "shared", "common.txt", "shared contents\n")
    link = base / "link"
    if not link.exists():
        link.symlink_to(base / "shared", target_is_directory=True)

    # A trailing slash transfers the directory's contents into the top of
    # the sandbox, so the directory's own name doesn't matter.
    placeholder, handles = submit_fake_dag(the_condor, test_dir, "dag_same_dir_contents", [
        {"initialdir": base.as_posix(), "transfer_common_input": "shared/", "leave_in_queue": True},
        {"initialdir": base.as_posix(), "transfer_common_input": "link/", "leave_in_queue": True},
    ])
    yield query_jobs(the_condor, handles), handles
    for h in handles + [placeholder]:
        h.remove()


class TestCommonFilesMismatchHold:

    def test_cluster_match_runs(self, cluster_match):
        assert len(cluster_match) == 2
        assert all(status_of(r) == JobStatus.COMPLETED for r in cluster_match)

    def test_cluster_mismatch_holds_one(self, cluster_mismatch):
        h = assert_one_held_one_completed(cluster_mismatch, SUBCODE_CLUSTER)
        assert "do not match" in h["HoldReason"]
        assert "same cluster" in h["HoldReason"]

    def test_dag_mismatch_holds_second(self, dag_mismatch):
        results, handles = dag_mismatch
        h = assert_second_node_held(results, handles, SUBCODE_DAG)
        assert "do not match" in h["HoldReason"]
        assert "same DAG" in h["HoldReason"]

    def test_dag_order_holds_second(self, dag_order):
        results, handles = dag_order
        h = assert_second_node_held(results, handles, SUBCODE_DAG_ORDER)
        assert "different order" in h["HoldReason"]

    def test_dag_same_files_runs(self, dag_same_files):
        results, _ = dag_same_files
        assert len(results) == 2
        assert all(status_of(r) == JobStatus.COMPLETED for r in results)

    def test_dag_same_dir_contents_runs(self, dag_same_dir_contents):
        results, _ = dag_same_dir_contents
        assert len(results) == 2
        assert all(status_of(r) == JobStatus.COMPLETED for r in results)
