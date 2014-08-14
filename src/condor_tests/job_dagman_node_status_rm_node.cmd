executable = ./job_dagman_node_status_rm_node.pl
output = job_dagman_node_status_rm_node.out
error = job_dagman_node_status_rm_node.err
arguments = "A $(DAGManJobId)"
universe = scheduler
# Note: we need getenv = true for the node job to talk to the schedd of
# the personal condor that's running the test.
getenv       = true
notification = NEVER
queue
