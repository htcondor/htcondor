executable   = ./job_dagman_retry_recovery-nodeC.pl
arguments    = $(DAGManJobId)
universe     = scheduler
output       = job_dagman_retry_recovery-nodeC.out
error        = job_dagman_retry_recovery-nodeC.err
log          = job_dagman_retry_recovery.log
getenv = CONDOR*,PATH
Notification = NEVER
queue
