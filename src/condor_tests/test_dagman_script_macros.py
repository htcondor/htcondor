#!/usr/bin/env pytest


from ornithology import *
import htcondor2 as htcondor
import os

@action
def writeExecutable(test_dir):
    file = test_dir / "exit.sh"
    with open(file, "w") as f:
        f.write("""#!/bin/bash\nexit $1\n""")
    os.chmod(str(file), 0o0777)
    return file

@action
def writeItemData(test_dir):
    good_data = test_dir / "good.data"
    with open(good_data, "w") as f:
        f.write("""0\n0\n0\n0\n""")
    bad_data = test_dir / "bad.data"
    with open(bad_data, "w") as f:
        f.write("""1\n1\n2\n3\n""")
    return (good_data, bad_data)

@action
def writeSubmitFile(test_dir, writeExecutable):
    file = test_dir / "job.sub"
    with open(file, "w") as f:
        f.write(f"""
executable = {writeExecutable}
arguments  = "$(code)"
log        = jobs.log
priority   = $(code) * 100
queue code from $(filename)
        """)
    return file

@action
def writePostScript(test_dir):
    file = test_dir / "check-macros.py"
    with open(file, "w") as f:
        f.write("""#!/usr/bin/env python3
import sys
import htcondor2 as htcondor

if len(sys.argv) != 21:
    print(f"Unexepected number of arguments: {len(sys.argv)}")
    sys.exit(1)

schedd = htcondor.Schedd()
ads = schedd.query(constraint=f"DAG_NodesTotal=!=UNDEFINED", projection=['ClusterId'])

if len(ads) > 1:
    print("Schedd Query returned more than one ad (expected only the DAGMan job)")
    for ad in ads:
        print(ad)
    sys.exit(1)

DAG_ID = ads[0]["ClusterId"]

ads = schedd.history(constraint=f"DAGManJobId=={DAG_ID}", projection=["ClusterId", "DAGNodeName"], match=1)

CLUSTER_ID = 0
NODE_NAME = "???"
for ad in ads:
    CLUSTER_ID = ad["ClusterId"]
    NODE_NAME = ad["DAGNodeName"]

EXPECTED = {
    "GOOD" : [
        "0", # Retry
        "1", # Max retries
        "0", # DAG status
        "0", # Failed count
        "0", # Futile count
        "0", # Done count
        "0", # Queued count
        "2", # Num nodes
        f"{DAG_ID}", # DAGMan job id
        f"{CLUSTER_ID}.3", # Job ID
        f"{CLUSTER_ID}", # Cluster id
        "4", # Num jobs
        "True", # Success
        "0", # Return
        "0", # Exit codes
        "0:4", # Exit frequencies
        "-1", # Pre Script return
        "0", # Num jobs aborted
    ],
    "BAD" : [
        "0", # Retry
        "0", # Max retries
        "0", # DAG status
        "0", # Failed count
        "0", # Futile count
        "1", # Done count
        "0", # Queued count
        "2", # Num nodes
        f"{DAG_ID}", # DAGMan job id
        f"{CLUSTER_ID}.3", # Job ID
        f"{CLUSTER_ID}", # Cluster id
        "4", # Num jobs
        "False", # Success
        {"3","2","1"}, # Return
        "1,2,3", # Exit codes
        "1:2,2:1,3:1", # Exit frequencies
        "-1", # Pre Script return
        "0", # Num jobs aborted
    ],
}

if NODE_NAME not in EXPECTED.keys():
    print(f"Unexpected node name '{NODE_NAME}'")
    sys.exit(1)

if NODE_NAME != sys.argv[1] or NODE_NAME != sys.argv[2]:
    print(f"Provided Script $NODE ({sys.argv[1]}) or $JOB ({sys.argv[2]}) does not match {NODE_NAME}")
    sys.exit(1)

for i, script_macro in enumerate(sys.argv[3:]):
    expected_val = EXPECTED[NODE_NAME][i]
    match = script_macro in expected_val if type(expected_val) is set else script_macro == expected_val
    if not match:
        print(f"EXPECTED[{NODE_NAME}][{i}] ({expected_val}) != {script_macro}")
        sys.exit(1)

print("Script arguments were expected values")

        """)
    os.chmod(str(file), 0o0777)
    return file

@action
def runDAG(default_condor, test_dir, writePostScript, writeSubmitFile, writeItemData):
    DAG_FILE = "test.dag"
    with open(DAG_FILE, "w") as f:
        f.write(f"""
JOB GOOD {writeSubmitFile}
JOB BAD {writeSubmitFile}

PARENT GOOD CHILD BAD

VARS GOOD PREPEND filename="{writeItemData[0]}"
VARS BAD PREPEND filename="{writeItemData[1]}"

RETRY GOOD 1

SCRIPT DEBUG script.debug ALL POST GOOD {writePostScript} $node $JOB $RETRY $MAX_RETRIES $DAG_STATUS $FAILED_COUNT $FUTILE_COUNT $DONE_COUNT $QUEUED_COUNT $NODE_COUNT $DAGID $JOBID $CLUSTERID $JOB_COUNT $SUCCESS $RETURN $EXIT_CODES $EXIT_CODE_COUNTS $PRE_SCRIPT_RETURN $JOB_ABORT_COUNT
SCRIPT DEBUG script.debug ALL POST BAD {writePostScript} $NoDe $JOB $RETRY $MAX_RETRIES $DAG_STATUS $FAILED_COUNT $FUTILE_COUNT $DONE_COUNT $QUEUED_COUNT $NODE_COUNT $DAGID $JOBID $CLUSTERID $JOB_COUNT $SUCCESS $RETURN $EXIT_CODES $EXIT_CODE_COUNTS $PRE_SCRIPT_RETURN $JOB_ABORT_COUNT
        """)

    assert os.path.exists(DAG_FILE)
    dag = htcondor.Submit.from_dag(str(DAG_FILE))
    return default_condor.submit(dag)


@action
def checkDAGExit(default_condor, runDAG):
    with default_condor.use_config():
        ads = htcondor.Schedd().history(f"ClusterId=={runDAG.clusterid}", projection=["ExitCode"], match=1)
        for ad in ads:
            return int(ad["ExitCode"])

class TestDAGManScriptMacros:
    def test_dagman_script_macros(self, runDAG):
        # POST scripts verify everything. As long as the DAG completes successfully then pass
        assert runDAG.wait(condition=ClusterState.all_complete, timeout=60)
        assert runDAG.state.all_status(jobs.JobStatus.COMPLETED)

    def test_verify_exit_code(self, checkDAGExit):
        assert checkDAGExit == 0

