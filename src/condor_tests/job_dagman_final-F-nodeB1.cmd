executable = ./job_dagman_final-F-node.pl
output = job_dagman_final-F-nodeB1.out
error = job_dagman_final-F-nodeB1.err
arguments = "F_B1 0 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
notification = NEVER
queue
