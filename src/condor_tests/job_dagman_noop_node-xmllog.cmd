executable   = ./job_dagman_noop_node-node_normal.pl
arguments    = $(nodename)
universe     = scheduler
output       = job_dagman_noop_node-$(nodename).out
error        = job_dagman_noop_node-$(nodename).err
log          = job_dagman_noop_node-nodeEF.log
log_xml      = true
Notification = NEVER
queue
