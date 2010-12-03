executable   = ./job_dagman_jobstate_log-nodeC.pl
arguments    = $(nodename)
universe     = scheduler
output       = $(nodename).out
error        = $(nodename).err
log          = job_dagman_jobstate_log-nodeC.log
Notification = NEVER
queue
