executable = ./job_dagman_final-E-nodeB2.pl
output = job_dagman_final-E-nodeB2.out
error = job_dagman_final-E-nodeB2.err
arguments = "E_B2 0 $(DAG_STATUS) $(FAILED_COUNT) $(DAGManJobId)"
universe = scheduler
# Note: we need getenv = true for the node job to talk to the schedd of
# the personal condor that's running the test.
getenv       = true
notification = NEVER
queue
