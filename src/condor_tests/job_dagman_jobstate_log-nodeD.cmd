executable   = job_dagman_jobstate_log-nodeD.pl
arguments    = $(DAGManJobId)
universe     = local
output       = job_dagman_jobstate_log-nodeD.out
error        = job_dagman_jobstate_log-nodeD.err
log          = job_dagman_jobstate_log-nodeD.log
getenv = CONDOR*,PATH
Notification = NEVER
queue
