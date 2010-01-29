executable   = ./job_dagman_noop_node-node_hold-release.pl
arguments    = $(nodename) $(DAGManJobId)
universe     = scheduler
output       = job_dagman_noop_node-$(nodename).out
error        = job_dagman_noop_node-$(nodename).err
# Note: we need getenv = true for the node job to talk to the schedd of
# the personal condor that's running the test.
getenv       = true
# Note: this submit file uses the DAGMan default log file feature.
Notification = NEVER
queue
