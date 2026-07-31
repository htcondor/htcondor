#!/usr/bin/env pytest

#   test_dag_resources.py
#
#   Test the 'htcondor dag resources' CLI verb.
#   Verifies that the command queries DAG node jobs and displays
#   per-job resource usage information (node name, job id, status,
#   memory usage, request memory, job starts, remote host).

from ornithology import *
import htcondor2
import time

DAG_FILE = "test.dag"
TIMEOUT = 90

#------------------------------------------------------------------
@standup
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "NUM_CPUS": "4",
        },
    ) as condor:
        yield condor

#------------------------------------------------------------------
@action
def write_dag(test_dir, path_to_sleep):
    short_sub = test_dir / "short.sub"
    with open(str(short_sub), "w") as f:
        f.write(
f"""executable = {path_to_sleep}
arguments  = 0
request_memory = 17
""")

    long_sub = test_dir / "long.sub"
    with open(str(long_sub), "w") as f:
        f.write(
f"""executable = {path_to_sleep}
arguments  = 3600
request_memory = 37
""")

    dag = test_dir / DAG_FILE
    with open(str(dag), "w") as f:
        f.write(
"""JOB NodeA short.sub
JOB NodeB short.sub
JOB NodeC long.sub
""")
    return dag

#------------------------------------------------------------------
@action
def dag_handle(condor, write_dag):
    dag = htcondor2.Submit.from_dag(str(write_dag))
    return condor.submit(dag)

#------------------------------------------------------------------
@action
def resources_output(condor, dag_handle):
    # Wait until NodeC is running
    schedd = condor.get_local_schedd()
    deadline = time.time() + TIMEOUT
    while time.time() < deadline:
        jobs = schedd.query(
            constraint=f'DAGManJobId == {dag_handle.clusterid} && DAGNodeName == "NodeC" && JobStatus == 2',
            projection=["ClusterId"],
        )
        if len(jobs) == 1:
            break
        time.sleep(1)
    else:
        raise RuntimeError("Timed out waiting for NodeC to be running")

    p = condor.run_command(["htcondor", "dag", "resources", f"{dag_handle.clusterid}"])

    # Remove the DAG so the long-sleeping job doesn't block teardown
    schedd.act(htcondor2.JobAction.Remove, f"ClusterId == {dag_handle.clusterid}")

    return p

#==================================================================
class TestDagResources:
    def test_command_succeeds(self, resources_output):
        assert resources_output.returncode == 0

    def test_header_present(self, resources_output):
        lines = resources_output.stderr.strip().splitlines()
        assert len(lines) > 0
        header = lines[0]
        assert "NODE_NAME" in header
        assert "JOB_ID" in header
        assert "STATUS" in header
        assert "MEM_USAGE" in header
        assert "REQ_MEM" in header
        assert "STARTS" in header
        assert "REMOTE_HOST" in header

    def test_running_node_shown(self, resources_output):
        output = resources_output.stderr
        assert "NodeC" in output

    def test_running_job_has_status_and_host(self, resources_output):
        lines = resources_output.stderr.strip().splitlines()
        node_c_lines = [l for l in lines if "NodeC" in l]
        assert len(node_c_lines) == 1
        line = node_c_lines[0]
        assert "Running" in line
        # Running job should have a remote host (not just "-")
        fields = line.split()
        assert fields[-1] != "-"

    def test_running_job_has_request_memory(self, resources_output):
        lines = resources_output.stderr.strip().splitlines()
        node_c_lines = [l for l in lines if "NodeC" in l]
        assert len(node_c_lines) == 1
        line = node_c_lines[0]
        # Should show request_memory = 37
        assert "37" in line
