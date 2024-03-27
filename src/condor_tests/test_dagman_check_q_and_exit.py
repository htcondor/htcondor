#!/usr/bin/env pytest

# This test (test_dagman_check_q_and_exit.py) verifies that
# DAGMan will check the local schedd queue for associated jobs
# and rescue/abort if a job pending nodes jobs is not found

from ornithology import *
import htcondor
import os

#--------------------------------------------------------------------------
@standup
def condor(test_dir):
    with Condor(local_dir=test_dir / "condor", config={
            "DAGMAN_USE_DIRECT_SUBMIT" : True,
            "DAGMAN_PENDING_REPORT_INTERVAL" : 0,
            "DAGMAN_CHECK_QUEUE_INTERVAL" : 1}) as condor:
        yield condor

#--------------------------------------------------------------------------
@action
def write_chmod_script():
    SCRIPT_NAME = "chmod.py"
    with open(SCRIPT_NAME, "w") as f:
        f.write("""#!/usr/bin/env python3
import sys
import os
import htcondor

ID = int(sys.argv[1])

schedd = htcondor.Schedd()
ads = schedd.query(f"ClusterId=={ID}", ["DAGManNodesLog"])

nodes_log = ads[0]["DAGManNodesLog"]
os.chmod(nodes_log, 0o0000)

""")
    os.chmod(SCRIPT_NAME, 0o0777)
    return SCRIPT_NAME

#--------------------------------------------------------------------------
@action
def write_dag(path_to_sleep, write_chmod_script):
    py_path = os.environ.get("PYTHONPATH")
    DAG_FILENAME = "test.dag"
    with open(DAG_FILENAME, "w") as f:
        f.write(f"""
JOB A {{
    universe   = local
    executable = {write_chmod_script}
    arguments  = $(ClusterId)
    error      = script.err
    log        = $(JOB).log
    getenv     = _CONDOR*, CONDOR_CONFIG
    environment= "PYTHONPATH={py_path}"
}}

JOB B {{
    executable = {path_to_sleep}
    arguments  = 0
    log        = $(JOB).log
}}

PARENT A CHILD B
""")
    return DAG_FILENAME

#--------------------------------------------------------------------------
@action
def run_dag(condor, write_dag):
    dag = htcondor.Submit.from_dag(write_dag)
    dagman_job = condor.submit(dag)
    assert dagman_job.wait(condition=ClusterState.all_complete, timeout=45)
    return write_dag

#--------------------------------------------------------------------------
RECOVERY_DAG = "recovery.dag"
@action
def write_recovery_log():
    with open(f"{RECOVERY_DAG}.nodes.log", "w") as f:
        f.write("""000 (999.000.000) 2024-03-27 12:20:00 Job submitted from host: <11.222.33.444:52722?addrs=11.222.33.444-52722&alias=random.host.name&noUDP&sock=schedd_29282_984b>
    DAG Node: RECOVERY
...
""")

#--------------------------------------------------------------------------
@action
def write_recovery_dag(path_to_sleep, write_recovery_log):
    with open(RECOVERY_DAG, "w") as f:
        f.write(f"""
JOB RECOVERY {{
    executable = {path_to_sleep}
    arguments  = 0
}}
""")

#--------------------------------------------------------------------------
@action
def run_recovery_dag(condor, write_recovery_dag):
    assert os.path.exists(f"{RECOVERY_DAG}.nodes.log")
    dag = htcondor.Submit.from_dag(RECOVERY_DAG, {"DoRecov" : True})
    dagman_job = condor.submit(dag)
    assert dagman_job.wait(condition=ClusterState.all_complete, timeout=30)
    return RECOVERY_DAG

#==========================================================================
class TestDAGManQueryCheck:
    def check_debug(self, dag_file):
        found_errmsg = False
        with open(str(dag_file) + ".dagman.out", "r") as f:
            for line in f:
                if "ERROR: DAGMan lost track the following jobs:" in line:
                    found_errmsg = True
                    break
        return found_errmsg

    def test_recovery_mode_files(self, run_recovery_dag):
        assert os.path.exists(str(run_recovery_dag) + ".rescue001")
        assert self.check_debug(run_recovery_dag)

    def test_run_dag(self, run_dag):
        assert True

    def test_check_files(self, run_dag):
        rescue_file = str(run_dag) + ".rescue001"
        assert os.path.exists(rescue_file)
        jobA_done = False
        with open(str(rescue_file), "r") as f:
            for line in f:
                if line[:6] == "DONE A":
                    jobA_done = True
                    break
        assert jobA_done
        assert self.check_debug(run_dag)
        assert os.stat("B.log").st_size != 0
