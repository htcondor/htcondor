executable = ./job_dagman_final-E-node.pl
output = job_dagman_final-E-nodeC.out
error = job_dagman_final-E-nodeC.err
arguments = "E_C 0 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
notification = NEVER
queue
