executable = ./job_dagman_final-I-node.pl
output = job_dagman_final-I-nodeA.out
error = job_dagman_final-I-nodeA.err
arguments = "I_A 0 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
notification = NEVER
queue
