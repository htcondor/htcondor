executable   = job_dagman_abnormal_term_recovery_retries-nodeA.pl
arguments    = $(DAGManJobId)
universe     = local
output       = job_dagman_abnormal_term_recovery_retries-nodeA.out
error        = job_dagman_abnormal_term_recovery_retries-nodeA.err
log          = job_dagman_abnormal_term_recovery_retries-nodeA.log
Notification = NEVER
queue
