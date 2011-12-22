executable = ./job_dagman_final-H-node.pl
output = job_dagman_final-H-nodeC.out
error = job_dagman_final-H-nodeC.err
arguments = "H_C 0 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
notification = NEVER
queue
