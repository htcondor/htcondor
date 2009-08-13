executable   = ./job_dagman_retry_recovery-nodeB.pl
universe     = scheduler
output       = job_dagman_retry_recovery-nodeB.out
error        = job_dagman_retry_recovery-nodeB.err
# Different path to the same log file...
log          = ../condor_tests/job_dagman_retry_recovery.log
Notification = NEVER
queue
