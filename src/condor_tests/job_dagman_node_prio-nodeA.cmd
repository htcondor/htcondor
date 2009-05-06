executable   = ./job_dagman_node_prio-nodeA.pl
arguments    = $(nodename)
universe     = scheduler
output       = $(job).out
error        = $(job).err
log          = job_dagman_node_prio.log
Notification = NEVER
queue
