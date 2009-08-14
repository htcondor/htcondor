executable   = ./job_dagman_node_dir-node.pl
arguments    = $(nodename)
universe     = scheduler
output       = job_dagman_node_dir-$(nodename).out
error        = job_dagman_node_dir-$(nodename).err
log          = job_dagman_node_dir.nodelog
Notification = NEVER
queue
