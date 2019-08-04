executable   = job_dagman_jobstate_log-nodeD.pl
arguments    = $(DAGManJobId)
universe     = local
output       = job_dagman_jobstate_log-nodeD.out
error        = job_dagman_jobstate_log-nodeD.err
log          = job_dagman_jobstate_log-nodeD.log
# Note: we need getenv = true for the node job to talk to the schedd of
# the personal condor that's running the test.
getenv       = true
Notification = NEVER
queue
