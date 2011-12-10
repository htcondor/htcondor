executable = ./job_dagman_final-C-node.pl
output = job_dagman_final-C-nodeB.out
error = job_dagman_final-C-nodeB.err
log = job_dagman_final-C-nodeB.log
arguments = "C_B 1 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
notification = NEVER
queue
