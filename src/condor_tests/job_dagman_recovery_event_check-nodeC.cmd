executable   = ./job_dagman_recovery_event_check-nodeC.pl
arguments    = $(DAGManJobId)
universe     = scheduler
output       = job_dagman_recovery_event_check-nodeC.out
error        = job_dagman_recovery_event_check-nodeC.err
log          = job_dagman_recovery_event_check-nodeC.log
# Note: we need getenv = true for the node job to talk to the schedd of
# the personal condor that's running the test.
getenv       = true
Notification = NEVER
queue
