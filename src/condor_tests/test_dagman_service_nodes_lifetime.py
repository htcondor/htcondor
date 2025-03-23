#!/usr/bin/env pytest

#-----------------------------------------------------------------------------------------------
#   test_dagman_service_nodes_lifetime.py
#
#   This test makes sure that service nodes live for the entire
#   life of DAGMan such that a user removing the DAG does not
#   remove the service node jobs when a FINAL node exists.
#
#   Author: Cole Bollig - 2025/03/07
#-----------------------------------------------------------------------------------------------

from ornithology import *
import htcondor2
import os
from time import time as now

#-----------------------------------------------------------------------------------------------
TIMEOUT = 30

NODE_NAME_SERVICE = "S"
NODE_NAME_WORKER  = "W"
NODE_NAME_FINAL   = "F"
DAG_FILE_NAME     = "test.dag"

#-----------------------------------------------------------------------------------------------
@action
def check_queue_script(test_dir):
    """Create script that checks for only the Service node job to be in the queue"""
    SCRIPT = test_dir / "check_queue.py"
    with open(SCRIPT, "w") as f:
        f.write(f"""#!/usr/bin/env python3
import os
import sys
import htcondor2

print("Checking arguments...")

if len(sys.argv) != 2:
    print(f"ERROR: Invalid number of args: {{len(sys.argv)}}")
    sys.exit(1)

dag_id = int(sys.argv[1])
print(f"DAG ID: {{dag_id}}")

print("Query Schedd for placed DAGMan jobs...")

schedd = htcondor2.Schedd()
ads = schedd.query(constraint=f"DAGManJobId=={{dag_id}}", projection=['DagNodeName', 'JobStatus'])

found = False

for ad in ads:
    if ad['DagNodeName'] != '{NODE_NAME_SERVICE}':
        print(f"ERROR: Unexpected node still in queue: {{ad['DagNodeName']}}")
        sys.exit(2)
    elif found:
        print("ERROR: Service node found twice!!!")
        sys.exit(3)
    elif int(ad['JobStatus']) not in [1,2]:
        print("ERROR: Service node not idle or running")
        sys.exit(4)
    else:
        found = True

if not found:
    print("ERROR: Service node not found")
    sys.exit(5)

print("Check successful")

sys.exit(0)
""")
    os.chmod(str(SCRIPT), 0o0777)
    return SCRIPT

#-----------------------------------------------------------------------------------------------
@action
def submit_dag(default_condor, path_to_sleep, check_queue_script, test_dir):
    DAG_CONTENTS = f"""
    SUBMIT-DESCRIPTION sleep {{
        executable  = {path_to_sleep}
        arguments   = $(WAIT_T)

        WAIT_T = 600

        queue
    }}

    SERVICE {NODE_NAME_SERVICE} sleep
    JOB {NODE_NAME_WORKER} sleep
    FINAL {NODE_NAME_FINAL} sleep

    VARS {NODE_NAME_FINAL} APPEND WAIT_T="3"
    SCRIPT DEBUG script.debug ALL POST {NODE_NAME_FINAL} {check_queue_script} $DAGID
    """

    # Create test dag
    DAG_FILE = test_dir / DAG_FILE_NAME
    with open(DAG_FILE, "w") as f:
        f.write(DAG_CONTENTS)

    DAG = htcondor2.Submit.from_dag(str(DAG_FILE))
    handle = default_condor.submit(DAG)

    # Wait for DAG to begin executing
    assert handle.wait(condition=ClusterState.all_running, timeout=TIMEOUT)

    # Wait for Worker not to have been submitted to remove DAG node scheduler job
    NODES_LOG = test_dir / f"{DAG_FILE}.nodes.log"
    START_T = now()
    JEL = None

    # Keep trying to open event log
    while now() - START_T < TIMEOUT:
        try:
            JEL = htcondor2.JobEventLog(str(NODES_LOG))
            break
        except htcondor2.HTCondorException:
            JEL = None

    assert JEL != None

    START_T = now()
    REMOVE_CALLED = False

    while not REMOVE_CALLED and now() - START_T < TIMEOUT:
        for event in JEL.events(stop_after=1):
            if event.type == htcondor2.JobEventType.SUBMIT:
                node = event["LogNotes"].split()[2]
                if node == NODE_NAME_WORKER:
                    default_condor.run_command(["condor_rm", handle.clusterid])
                    REMOVE_CALLED = True
                    break

    assert REMOVE_CALLED

    return (handle, NODES_LOG)

#-----------------------------------------------------------------------------------------------
@action
def dag_handle(submit_dag):
    return submit_dag[0]

@action
def nodes_log(submit_dag):
    return str(submit_dag[1])

#-----------------------------------------------------------------------------------------------
class NodeInfo():
    def __init__(self, name: str):
        self.name = name
        self.executed = name == NODE_NAME_WORKER
        self.exited = False

    def __bool__(self):
        return self.executed and self.exited

#===============================================================================================
class TestDAGManServieNodeLifetime:
    def test_service_node_lifetime(self, dag_handle):
        """Wait for removal of DAG to be complete (i.e. SERVICE node actually removed)"""
        assert dag_handle.wait(condition=ClusterState.all_terminal, timeout=TIMEOUT)

    def test_node_states(self, nodes_log):
        """Check all expected node states and event logs"""
        NODE_INFO = dict()
        START_T = now()

        JEL = htcondor2.JobEventLog(nodes_log)
        while True:
            assert now() - START_T < TIMEOUT

            for event in JEL.events(stop_after=1):
                if event.type == htcondor2.JobEventType.SUBMIT:
                    node = event["LogNotes"].split()[2]
                    NODE_INFO[event.cluster] = NodeInfo(node)
                elif event.type == htcondor2.JobEventType.EXECUTE:
                    assert event.cluster in NODE_INFO
                    NODE_INFO[event.cluster].executed = True
                elif event.type == htcondor2.JobEventType.JOB_ABORTED:
                    assert event.cluster in NODE_INFO
                    assert NODE_INFO[event.cluster].name in [NODE_NAME_SERVICE, NODE_NAME_WORKER]
                    assert "DAG" in event["Reason"]
                    NODE_INFO[event.cluster].exited = True
                elif event.type == htcondor2.JobEventType.JOB_TERMINATED:
                    assert event.cluster in NODE_INFO
                    assert NODE_INFO[event.cluster].name == NODE_NAME_FINAL
                    assert event["ReturnValue"] == 0
                    NODE_INFO[event.cluster].exited = True

            if all(NODE_INFO.values()):
                break


