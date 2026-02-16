executable = ./job_dagman_final-E-nodeB2.pl
output = job_dagman_final-E-nodeB2.out
error = job_dagman_final-E-nodeB2.err
arguments = "E_B2 0 $(DAG_STATUS) $(FAILED_COUNT) $(DAGManJobId)"
universe = scheduler
getenv = CONDOR*,PATH
notification = NEVER
queue
