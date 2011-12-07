executable = ./job_dagman_final-E-nodeB1.pl
output = job_dagman_final-E-nodeB1.out
error = job_dagman_final-E-nodeB1.err
arguments = "E_B1 0 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
notification = NEVER
queue
