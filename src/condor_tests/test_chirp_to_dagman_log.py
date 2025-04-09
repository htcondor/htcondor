#!/usr/bin/env pytest

# This test is a basic smoke test to check that the HTChirp ulog
# events (sadly Generic type [8]) are recorded in the DAGMan nodes
# log.

from ornithology import *
import htcondor2 as htcondor
import os
from time import time


DAG_FILENAME = "simple.dag"
#--------------------------------------------------------------------------
@action
def write_files():
    # Write script to chirp a ulog message
    CHIRP_SCRIPT_NAME = "chirp.py"
    with open(CHIRP_SCRIPT_NAME, "w") as f:
        f.write("""#!/usr/bin/python3
from htcondor2.htchirp import HTChirp
import sys

msg = "Error: No message provided!"
if len(sys.argv) > 1:
    msg = str(sys.argv[1])

with HTChirp() as chirp:
    chirp.ulog(f"{msg}")
""")
    os.chmod(CHIRP_SCRIPT_NAME, 0o777)

    # Get python path to set in job environment so chirp py bindings work
    py_path = os.environ.get("PYTHONPATH")
    assert py_path != None

    # Base job desctiption for both jobs
    description = {
        "executable" : CHIRP_SCRIPT_NAME,
        "error"      : "chirp-$(dag_node_name).err",
        "log"        : "chirp-$(dag_node_name).log",
        "env"        : f'"PYTHONPATH={py_path}"',
        "want_io_proxy" : "true",
    }

    # Write a job that has its own job log
    with open("with_log.sub", "w") as f:
        for cmd, info in description.items():
            f.write(f"{cmd} = {info}\n")
        f.write("""arguments = "'In both job & DAGMan logs?'"\n""")
        f.write("\nqueue\n")

    # Write a job with no personal log
    if "log" in description:
        del description["log"]
    with open("no_log.sub", "w") as f:
        for cmd, info in description.items():
            f.write(f"{cmd} = {info}\n")
        f.write("""arguments = "'Only in DAGMan log?'"\n""")
        f.write("\nqueue\n")

    # Write simple DAG to run
    with open(DAG_FILENAME, "w") as f:
        f.write("JOB HAS_LOG with_log.sub\n")
        f.write("JOB NO_LOG  no_log.sub\n")

#--------------------------------------------------------------------------
@action
def run_dag(default_condor, test_dir, write_files):
    dag = htcondor.Submit.from_dag(DAG_FILENAME)
    dagman_job = default_condor.submit(dag)
    assert dagman_job.wait(condition=ClusterState.all_complete, timeout=80)
    return DAG_FILENAME + ".nodes.log"

#==========================================================================
class TestChirpToDAGMan:
    def test_log_existence(self, run_dag):
        # Check expected logs exist and unexpected logs don't
        assert os.path.exists(run_dag)
        assert os.path.exists("chirp-HAS_LOG.log")
        assert not os.path.exists("chirp-NO_LOG.log")

    def test_info_in_job_log(self, run_dag):
        # Check that expected chirp message is in expected job log
        info = ""
        with htcondor.JobEventLog("chirp-HAS_LOG.log") as log:
            begin = time()
            for event in log.events(stop_after=None):
                if event.type is htcondor.JobEventType.GENERIC:
                    info = event.get("Info", "")
                    break
                assert time() - begin < 30
        assert info == "In both job & DAGMan logs?"

    def test_chirp_in_dag_log(self, run_dag):
        # Check the *.nodes.log for correct number of chirp events
        expected_msgs = ["Only in DAGMan log?", "In both job & DAGMan logs?"]
        num_chirp_events = 0
        with htcondor.JobEventLog(run_dag) as log:
            num_termination = 0
            begin = time()
            for event in log.events(stop_after=None):
                if event.type is htcondor.JobEventType.GENERIC:
                    info = event.get("Info", "")
                    if "Error" in info:
                        print(f"\nUnexpected chirp message: {info}")
                    elif info in expected_msgs:
                        num_chirp_events += 1
                        expected_msgs.remove(info)
                elif event.type is htcondor.JobEventType.JOB_TERMINATED:
                    num_termination += 1
                if num_termination == 2:
                    break
                # Fail if termination events are missing
                assert time() - begin < 30
        if len(expected_msgs) != 0:
            print(f"\nError: Missing {expected_msgs} for chirp events in DAGMan log.")
            assert len(expected_msgs) == 0
        assert num_chirp_events == 2

