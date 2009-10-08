executable   = ./job_dagman_retry_recovery-nodeB2.pl
universe     = scheduler
output       = job_dagman_retry_recovery-nodeB2.out
error        = job_dagman_retry_recovery-nodeB2.err
# Use a different log file, just to stress things a little more.
log          = job_dagman_retry_recovery2.log
Notification = NEVER
queue
