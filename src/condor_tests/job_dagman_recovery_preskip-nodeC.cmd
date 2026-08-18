executable   = ./job_dagman_recovery_preskip-nodeC.pl
arguments    = $(DAGManJobId)
universe     = scheduler
output       = job_dagman_recovery_preskip-nodeC.out
error        = job_dagman_recovery_preskip-nodeC.err
log          = job_dagman_recovery_preskip-nodeC.log
getenv = CONDOR*,PATH
Notification = NEVER
queue
