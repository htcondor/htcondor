executable   = ./job_dagman_noop_node-node_conditional.pl
arguments    = $(nodename)
initialdir   = job_dagman_noop_node-subdir
universe     = scheduler
output       = job_dagman_noop_node-$(nodename).out
error        = job_dagman_noop_node-$(nodename).err
log          = job_dagman_noop_node-initialdir.log
Notification = NEVER
queue
