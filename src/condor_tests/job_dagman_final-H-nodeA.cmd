executable = ./job_dagman_final-H-node.pl
output = job_dagman_final-H-nodeA.out
error = job_dagman_final-H-nodeA.err
arguments = "H_A 0 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
notification = NEVER
queue
