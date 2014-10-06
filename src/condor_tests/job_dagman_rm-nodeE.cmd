universe = scheduler
executable = ./job_dagman_rm-nodeE.pl
arguments = "job_dagman_rm-NodeE-job $(DAGManJobId)"
log = job_dagman_rm-nodeE.log
output = job_dagman_rm-nodeE.out
error = job_dagman_rm-nodeE.err
# Note: we need getenv = true for the node job to talk to the schedd of
# the personal condor that's running the test.
getenv       = true
notification = NEVER
queue
