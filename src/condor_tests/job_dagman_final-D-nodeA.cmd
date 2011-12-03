executable = ./job_dagman_final-D-node.pl
output = job_dagman_final-D-nodeA.out
error = job_dagman_final-D-nodeA.err
log = job_dagman_final-D-nodeA.log
arguments = "A_A 0 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
queue
