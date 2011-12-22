executable = ./job_dagman_final-F-node.pl
output = job_dagman_final-F-nodeB2.out
error = job_dagman_final-F-nodeB2.err
arguments = "F_B2 2 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
notification = NEVER
queue
