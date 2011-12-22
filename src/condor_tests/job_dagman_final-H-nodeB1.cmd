executable = ./job_dagman_final-H-node.pl
output = job_dagman_final-H-nodeB1.out
error = job_dagman_final-H-nodeB1.err
arguments = "H_B1 0 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
notification = NEVER
queue
