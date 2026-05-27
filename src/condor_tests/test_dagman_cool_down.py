#!/usr/bin/env pytest

from ornithology import *
import htcondor2
import os
import stat
import time
from pathlib import Path

DAG_FILE = "test.dag"
TIMEOUT = 60

#-----------------------------------------------------------------------------------------
@action
def prepare_unwritable_log(test_dir) -> Path:
    """Prepare DAGMan debug log to be unwritable forcing a crash"""
    log = test_dir / f"{DAG_FILE}.dagman.out"

    if log.exists():
        os.remove(log)
    log.touch()

    os.chmod(log, stat.S_IREAD | stat.S_IRGRP | stat.S_IROTH)

    return log

#-----------------------------------------------------------------------------------------
@action
def dag_cool_down(default_condor, path_to_sleep, prepare_unwritable_log):
    # Write test DAG
    with open(DAG_FILE, "w") as f:
        f.write(f"""
#SET_JOB_ATTR DisableCoolDown = True
JOB sleep @=desc
    executable = {path_to_sleep}
    arguments  = 0
@desc
""")

    # Submit DAG
    DAG = htcondor2.Submit.from_dag(DAG_FILE)
    HANDLE = default_condor.submit(DAG)

    # Wait for DAG to execute and then return back to queue
    assert HANDLE.wait(condition=ClusterState.all_running, timeout=TIMEOUT)
    assert HANDLE.wait(condition=ClusterState.all_idle, timeout=TIMEOUT)

    # Get DAG record
    ads = HANDLE.query(projection=["JobCoolDownExpiration"])

    # Remove DAG since it will never actually execute
    HANDLE.remove()

    return ads

#-----------------------------------------------------------------------------------------
@action
def dag_fail_to_start_cool_down(default_condor, path_to_sleep, test_dir):
    # Create a subdirectory so from_dag sets IWD to it
    work_dir = test_dir / "work_dir"
    work_dir.mkdir()

    dag_file = work_dir / DAG_FILE
    with open(dag_file, "w") as f:
        f.write(f"""
JOB sleep_job @=desc
    executable = {path_to_sleep}
    arguments  = 0
@desc
""")

    # Submit on hold so we can rename the IWD before the job starts
    DAG = htcondor2.Submit.from_dag(str(dag_file))
    DAG["hold"] = "true"
    HANDLE = default_condor.submit(DAG)

    # Rename so the log is accessible for the pre-release check below
    renamed_dir = work_dir.rename(test_dir / "work_dir_renamed")

    # Recreate the original IWD path but strip all permissions so the schedd
    # cannot enter it, causing CreateProcessNew to fail
    work_dir.mkdir()
    os.chmod(work_dir, stat.S_IREAD | stat.S_IRGRP | stat.S_IROTH)

    try:
        # Verify DAGMan never started execution while on hold
        dag_log = renamed_dir / f"{DAG_FILE}.dagman.log"
        with htcondor2.JobEventLog(str(dag_log)) as jel:
            event_types = [e.type for e in jel.events(stop_after=0)]
        assert event_types == [htcondor2.JobEventType.SUBMIT]

        # Release; schedd will attempt to start DAGMan, fail (bad IWD),
        # and the new code in start_sched_universe_job sets the cool-down
        HANDLE.release()

        # Poll for JobCoolDownExpiration (job stays IDLE but gets attribute set)
        ads = []
        deadline = time.time() + TIMEOUT
        while time.time() < deadline:
            ads = HANDLE.query(projection=["JobCoolDownExpiration"])
            if ads and ads[0].get("JobCoolDownExpiration") is not None:
                break
            time.sleep(2)

        HANDLE.remove()
    finally:
        os.chmod(work_dir, stat.S_IRWXU)

    return ads

#=========================================================================================
class TestDAGManCoolDown:
    def test_dag_cool_down(self, dag_cool_down):
        """Verify job cool down was placed on DAGMan job"""
        assert len(dag_cool_down) == 1
        assert dag_cool_down[0].get("JobCoolDownExpiration") != None

    def test_dag_fail_to_start_cool_down(self, dag_fail_to_start_cool_down):
        """Verify cool down is set when DAGMan fails to start due to missing IWD"""
        assert len(dag_fail_to_start_cool_down) == 1
        assert dag_fail_to_start_cool_down[0].get("JobCoolDownExpiration") is not None
