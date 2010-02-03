executable   = ./job_dagman_noop_node-node_setup.pl
arguments    = $(nodename)
universe     = scheduler
output       = job_dagman_noop_node-$(nodename).out
error        = job_dagman_noop_node-$(nodename).err
# Note: this submit file uses the DAGMan default log file feature.
Notification = NEVER
queue
