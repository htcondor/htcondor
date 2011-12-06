executable = ./job_dagman_final-F-node.pl
output = job_dagman_final-F-nodeA.out
error = job_dagman_final-F-nodeA.err
arguments = "F_A 0 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
notification = NEVER
queue
