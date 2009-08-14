executable   = ./job_dagman_node_dir-nodeB.pl
arguments    = $(nodename)
universe     = scheduler
output       = job_dagman_node_dir-$(nodename).out
error        = job_dagman_node_dir-$(nodename).err
log          = job_dagman_node_dir-nodeB.nodelog
Notification = NEVER
queue
