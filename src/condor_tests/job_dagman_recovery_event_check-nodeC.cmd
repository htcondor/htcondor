executable   = ./job_dagman_recovery_event_check-nodeC.pl
arguments    = $(DAGManJobId)
universe     = scheduler
output       = job_dagman_recovery_event_check-nodeC.out
error        = job_dagman_recovery_event_check-nodeC.err
log          = job_dagman_recovery_event_check-nodeC.log
getenv = CONDOR*,PATH
Notification = NEVER
queue
