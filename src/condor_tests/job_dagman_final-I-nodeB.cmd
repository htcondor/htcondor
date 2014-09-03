executable = ./job_dagman_final-I-node.pl
output = job_dagman_final-I-nodeB.out
error = job_dagman_final-I-nodeB.err
arguments = "I_B 0 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
notification = NEVER
queue
