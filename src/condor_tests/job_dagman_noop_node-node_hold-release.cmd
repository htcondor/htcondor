executable   = ./job_dagman_noop_node-node_hold-release.pl
arguments    = $(nodename) $(DAGManJobId)
universe     = scheduler
output       = job_dagman_noop_node-$(nodename).out
error        = job_dagman_noop_node-$(nodename).err
getenv = CONDOR*,PATH
# Note: this submit file uses the DAGMan default log file feature.
Notification = NEVER
queue
