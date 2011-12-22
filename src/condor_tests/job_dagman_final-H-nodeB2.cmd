executable = ./job_dagman_final-H-node.pl
output = job_dagman_final-H-nodeB2.out
error = job_dagman_final-H-nodeB2.err
arguments = "H_B2 0 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
notification = NEVER
queue
