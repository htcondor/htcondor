#!/usr/bin/env pytest

# Regression test for a DAGMan crash (uncaught std::length_error -> SIGABRT)
# caused by "transfer shadow" events leaking into the shared node event log.
#
# The container-common-files feature (container_is_common) has the schedd
# spawn a "transfer shadow" that is tracked internally with a synthetic
# negative procID (procID <= -1000, see FIRST_TRANSFER_PROC_ID in
# src/condor_utils/transfer_proc.h). That helper reuses the real job's
# ClassAd wholesale (including ATTR_DAGMAN_WORKFLOW_LOG), so its own events
# (e.g. a JobEvictedEvent for the transfer shadow itself, not the real job)
# end up written into the DAG's shared nodes.log alongside the real node's
# job events.
#
# DAGMan's event-to-node lookup (Dag::FindNodeByEventID) keys on cluster ID
# only, ignoring proc, so such an event resolves to the real Node just like
# any of its genuine job-proc events would. Node::CheckTrackingJob then
# tries to resize a per-node std::vector<JobDetails> by (proc+1) -- for
# proc=-1000 that converts to a huge size_t and throws, aborting DAGMan.
#
# This test replays a hand-crafted nodes.log containing exactly that
# pattern -- a real submit/execute, a spurious transfer-shadow eviction
# event for the same cluster, then a real successful terminate -- through
# DAGMan in recovery mode ("-DoRecov") and verifies DAGMan ignores the
# bogus event and completes the DAG instead of crashing.

from ornithology import *
import htcondor2 as htcondor
import os

DAG_FILENAME = "shadow_event.dag"
NODES_LOG = f"{DAG_FILENAME}.nodes.log"
REAL_CLUSTER = 1
TRANSFER_SHADOW_PROC = -1000  # FIRST_TRANSFER_PROC_ID, src/condor_utils/transfer_proc.h

#--------------------------------------------------------------------------
@standup
def condor(test_dir):
    # If DAGMan aborts (SIGABRT/signal 6) while processing the bogus
    # transfer-shadow event, its scheduler-universe job would otherwise be
    # left stuck in the queue. The schedd sets ExitBySignal/ExitSignal on a
    # scheduler-universe job's ad and evaluates SYSTEM_PERIODIC_REMOVE
    # synchronously in its exit reaper (schedd.cpp's
    # scheduler_universe_job_exit -> UserPolicy::AnalyzePolicy with
    # PERIODIC_THEN_EXIT), so this fires immediately on the crash rather
    # than waiting for the next periodic policy sweep.
    with Condor(local_dir=test_dir / "condor", config={
            "SYSTEM_PERIODIC_REMOVE": "(ExitBySignal =?= true && ExitSignal =?= 6)",
    }) as condor:
        yield condor

#--------------------------------------------------------------------------
@action
def write_nodes_log():
    with open(NODES_LOG, "w") as f:
        f.write(f"""000 ({REAL_CLUSTER}.000.000) 2024-01-01 00:00:00 Job submitted from host: <11.222.33.444:52722?addrs=11.222.33.444-52722&alias=random.host.name&noUDP&sock=schedd_29282_984b>
    DAG Node: A
...
001 ({REAL_CLUSTER}.000.000) 2024-01-01 00:00:01 Job executing on host: <11.222.33.444:52722?addrs=11.222.33.444-52722&alias=random.host.name&noUDP&sock=startd_1234_abcd>
...
004 ({REAL_CLUSTER}.{TRANSFER_SHADOW_PROC}.000) 2024-01-01 00:00:02 Job was evicted.
	(0) CPU times
		Usr 0 00:00:00, Sys 0 00:00:00  -  Run Remote Usage
		Usr 0 00:00:00, Sys 0 00:00:00  -  Run Local Usage
	0  -  Run Bytes Sent By Job
	0  -  Run Bytes Received By Job
	Reason: Job not found at execution machine
...
005 ({REAL_CLUSTER}.000.000) 2024-01-01 00:00:03 Job terminated.
	(1) Normal termination (return value 0)
		Usr 0 00:00:00, Sys 0 00:00:00  -  Run Remote Usage
		Usr 0 00:00:00, Sys 0 00:00:00  -  Run Local Usage
		Usr 0 00:00:00, Sys 0 00:00:00  -  Total Remote Usage
		Usr 0 00:00:00, Sys 0 00:00:00  -  Total Local Usage
	0  -  Run Bytes Sent By Job
	0  -  Run Bytes Received By Job
	0  -  Total Bytes Sent By Job
	0  -  Total Bytes Received By Job
...
""")
    return NODES_LOG

#--------------------------------------------------------------------------
@action
def write_dag(path_to_sleep, write_nodes_log):
    # Node A's submit description is never actually used: the hand-written
    # log above already shows it fully (and successfully) terminated, so
    # DAGMan recovery reconstructs it as DONE without resubmitting anything.
    with open(DAG_FILENAME, "w") as f:
        f.write(f"""
JOB A {{
    executable = {path_to_sleep}
    arguments  = 0
}}
""")
    return DAG_FILENAME

#--------------------------------------------------------------------------
@action
def run_dag(condor, write_dag):
    assert os.path.exists(NODES_LOG)
    dag = htcondor.Submit.from_dag(write_dag, {"DoRecov": True})
    dagman_job = condor.submit(dag)
    # fail_condition lets wait() return promptly once SYSTEM_PERIODIC_REMOVE
    # (see the condor fixture) removes a crashed DAGMan job, instead of
    # sitting through the full timeout.
    completed = dagman_job.wait(
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_terminal,
        timeout=120,
    )
    return dagman_job, completed, write_dag

#==========================================================================
class TestDAGManIgnoresTransferShadowEvents:
    def test_dagman_survives_transfer_shadow_event(self, run_dag):
        dagman_job, completed, dag_file = run_dag
        dagman_out = f"{dag_file}.dagman.out"
        assert os.path.exists(dagman_out)

        with open(dagman_out, "r") as f:
            out = f.read()

        # DAGMan must not have aborted while processing the bogus event.
        assert "Caught signal" not in out
        assert completed

        # DAGMan should have recognized node A as done from the real
        # submit/execute/terminate events and finished the DAG successfully,
        # not bailed out into a rescue file.
        assert not os.path.exists(f"{dag_file}.rescue001")

        # If DAGMan had crashed, SYSTEM_PERIODIC_REMOVE should have already
        # cleaned its job out of the queue rather than leaving it stuck.
        assert not dagman_job.state.any_status(JobStatus.REMOVED)
