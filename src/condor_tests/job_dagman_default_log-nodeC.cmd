executable   = job_dagman_default_log-nodeC.pl
arguments    = $(DAGManJobId)
universe     = local
output       = job_dagman_default_log-nodeC.out
error        = job_dagman_default_log-nodeC.err
log          = job_dagman_default_log-nodeC.log
getenv = CONDOR*,PATH
Notification = NEVER
queue
