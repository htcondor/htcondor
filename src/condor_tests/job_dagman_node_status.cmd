executable   = ./job_dagman_node_status.pl
arguments    = $(JOB) $(copystatus)
universe     = scheduler
output       = job_dagman_node_status-$(JOB).out
error        = job_dagman_node_status-$(JOB).err
Notification = NEVER
queue
