executable   = ./job_dagman_script_defer-nodeD.pl
arguments    = $(DAGManJobId)
universe     = scheduler
output       = job_dagman_script_defer-nodeD.out
error        = job_dagman_script_defer-nodeD.err
log          = job_dagman_script_defer-nodeD.log
# Note: we need getenv = true for the node job to talk to the schedd of
# the personal condor that's running the test.
getenv       = true
Notification = NEVER
queue
