executable   = ./job_dagman_recovery_preskip-nodeC.pl
arguments    = $(DAGManJobId)
universe     = scheduler
output       = job_dagman_recovery_preskip-nodeC.out
error        = job_dagman_recovery_preskip-nodeC.err
log          = job_dagman_recovery_preskip-nodeC.log
# Note: we need getenv = true for the node job to talk to the schedd of
# the personal condor that's running the test.
getenv       = true
Notification = NEVER
queue
