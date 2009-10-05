executable   = ./job_dagman_multi_dag-node.pl
arguments    = $(nodename)
universe     = scheduler
output       = job_dagman_multi_dag-$(nodename)-job.out
error        = job_dagman_multi_dag-node.err
log          = job_dagman_multi_dag-node.log
Notification = NEVER
queue
