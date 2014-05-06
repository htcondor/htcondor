executable = ./job_dagman_final-I-node.pl
output = job_dagman_final-I-nodeC.out
error = job_dagman_final-I-nodeC.err
arguments = "I_C 0 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
notification = NEVER
queue
