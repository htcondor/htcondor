#!/usr/bin/env pytest

from ornithology import *
import htcondor2
import os
import stat
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

#=========================================================================================
class TestDAGManCoolDown:
    def test_dag_cool_down(self, dag_cool_down):
        """Verify job cool down was placed on DAGMan job"""
        assert len(dag_cool_down) == 1
        assert dag_cool_down[0].get("JobCoolDownExpiration") != None
