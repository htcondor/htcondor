executable   = job_dagman_default_log-nodeC.pl
arguments    = $(DAGManJobId)
universe     = local
output       = job_dagman_default_log-nodeC.out
error        = job_dagman_default_log-nodeC.err
log          = job_dagman_default_log-nodeC.log
# Note: we need getenv = true for the node job to talk to the schedd of
# the personal condor that's running the test.
getenv       = true
Notification = NEVER
queue
