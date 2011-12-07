executable = ./job_dagman_final-F-node.pl
output = job_dagman_final-F-nodeC.out
error = job_dagman_final-F-nodeC.err
arguments = "F_C 0 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
notification = NEVER
queue
