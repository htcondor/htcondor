#!/usr/bin/env pytest

#   test_futile_node_inefficiency.py
#
#   LIGO discovered that with very large DAGs with many wide layers
#   of nodes can result in DAGMan hanging on the recursive function
#   that sets a nodes descendants to FUTILE status. This was due to
#   the function being inefficient and always checking calling this
#   function for all of its children when in theory if a child is
#   already futile then we do not need to traverse its children.
#   This test is to make sure a regression to this behavior does
#   not occur.
#   Author: Cole Bollig - 2023-07-21


from ornithology import *
import htcondor2 as htcondor
import os
from math import ceil

#------------------------------------------------------------------
# Function to increment are name letter sequence indexes
def increment(seq=[], pos=-1):
    if pos == -1:
        return
    seq[pos] = seq[pos] + 1
    if seq[pos] == 26:
        seq[pos] = 0
        increment(seq, pos-1)

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# Function to generate a shiskabob DAG with various width layers and designated failure points (nodes)
def generate_dag(filename="", job_submit="", fail_script="", depth=1, layers={}, failure_points={}):
    # Characters used for making a unique node name
    ALPHA = ["A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z"]
    # Open DAG file
    with open(filename, "w") as f:
        # Make node name sequence
        node_seq = [0] * ceil(depth / 26)
        fail_lines = []
        # For each layer in the DAG cake
        for i in range(depth):
            node = ""
            # Construct base node name for this layer
            for n in range(len(node_seq)):
                node = node + ALPHA[node_seq[n]]
            # Layer is not multi nodes wide (i.e. single node)
            if i not in layers.keys():
                f.write(f"JOB {node} {job_submit}\n")
            else:
                # Layer has multi node width
                for j in range(layers[i]):
                    # Append node number to make unique node name
                    node_i = node + str(j)
                    f.write(f"JOB {node_i} {job_submit}\n")
            # Check if this layer has failure nodes
            if i in failure_points.keys():
                # Failure point is on a single node layer
                if i not in layers.keys():
                    fail_lines.append(f"SCRIPT POST {node} {fail_script}\n")
                else:
                    # Failure point is on a multi node layer
                    for j in range(layers[i]):
                        # Failure point can be list of node #'s, single node #, or 'ALL'
                        node_i = node + str(j)
                        if type(failure_points[i]) == list and j in failure_points[i]:
                            fail_lines.append(f"SCRIPT POST {node_i} {fail_script}\n")
                        elif failure_points[i] == "ALL" or failure_points[i] == j:
                            fail_lines.append(f"SCRIPT POST {node_i} {fail_script}\n")
            # increment the node name sequence for next unique node name
            increment(node_seq, len(node_seq)-1)
        # write a break lins
        f.write("\n")
        # write all post scripts for failure points
        for line in fail_lines:
            f.write(line)
        # write a break line
        f.write("\n")
        # Reset node sequence to start for parent/child relationships
        for i in range(len(node_seq)):
            node_seq[i] = 0
        parents = []
        # Write Parent/child relationships
        for i in range(depth):
            write = False
            # Write out parents
            if len(parents) > 0:
                write = True
                f.write("PARENT")
                for parent in parents:
                    f.write(f" {parent}")
                f.write(" CHILD")
            # Empty parents because the children will become parents for next layer
            parents.clear()
            # Make node name
            node = ""
            for n in range(len(node_seq)):
                node = node + ALPHA[node_seq[n]]
            # Add node(s) to parents list for next line
            # Also write nodes if we have written out their parents above
            if i not in layers.keys():
                if write:
                    f.write(f" {node}")
                parents.append(node)
            else:
                for j in range(layers[i]):
                    node_i = node + str(j)
                    if write:
                        f.write(f" {node_i}")
                    parents.append(node_i)
            if write:
                f.write("\n")
            increment(node_seq, len(node_seq)-1)

#------------------------------------------------------------------
# Write submit file to be used by all nodes in DAG
@action
def submit_file(test_dir, path_to_sleep):
    path = os.path.join(str(test_dir), "job.sub")
    with open(path,"w") as f:
        f.write(f"""# Simple sleep job
executable = {path_to_sleep}
arguments  = 0
log        = job.log
queue""")
    return path

#------------------------------------------------------------------
# Write a script that will cause specified nodes to fail
@action
def fail_script(test_dir):
    path = os.path.join(str(test_dir), "fail.sh")
    with open(path,"w") as f:
        f.write("""#!/usr/bin/env python3
exit(1)""")
    os.chmod(path, 0o777)
    return path

#------------------------------------------------------------------
# Run test here: generate DAG based on parameters and files and submit DAG
@action
def submit_large_dag(default_condor, test_dir, submit_file, fail_script):
    dag_file_path = os.path.join(str(test_dir), "generated.dag")
    layers = { 0:2, 1:100, 3:100, 5:300, 7:100, 10:50, 15:50, 20:75, 30:100, 40:150, 50:300 }
    failures = { 0:"ALL" }
    generate_dag(dag_file_path, submit_file, fail_script, 52, layers, failures)
    dag = htcondor.Submit.from_dag(dag_file_path)
    return default_condor.submit(dag)

#==================================================================
class TestDAGManFutileNodesFnc:
    def test_futile_node_efficiency(self, submit_large_dag):
        # Make sure we are not taking a long time recursing because of re-traversal of nodes
        # This will either finish fast or take a very long time
        assert submit_large_dag.wait(condition=ClusterState.all_complete,timeout=80)

