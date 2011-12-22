executable = ./job_dagman_final-G-node.pl
output = job_dagman_final-G-nodeC.out
error = job_dagman_final-G-nodeC.err
arguments = "G_A 0 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
notification = NEVER
queue
