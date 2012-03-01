executable = ./job_dagman_final-E-node.pl
output = job_dagman_final-E-nodeA.out
error = job_dagman_final-E-nodeA.err
arguments = "E_A 0 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
notification = NEVER
queue
