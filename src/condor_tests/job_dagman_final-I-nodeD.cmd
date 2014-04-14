executable = ./job_dagman_final-I-node.pl
output = job_dagman_final-I-nodeD.out
error = job_dagman_final-I-nodeD.err
arguments = "I_D 0 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
notification = NEVER
queue
