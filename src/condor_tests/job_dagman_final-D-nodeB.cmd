executable = ./job_dagman_final-D-node.pl
output = job_dagman_final-D-nodeB.out
error = job_dagman_final-D-nodeB.err
log = job_dagman_final-D-nodeB.log
arguments = "D_B 1 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
notification = NEVER
queue
