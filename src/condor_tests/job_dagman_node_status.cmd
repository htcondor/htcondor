executable   = ./job_dagman_node_status.pl
arguments    = $(nodename)
universe     = scheduler
output       = job_dagman_node_status-$(nodename).out
error        = job_dagman_node_status-$(nodename).err
Notification = NEVER
queue
