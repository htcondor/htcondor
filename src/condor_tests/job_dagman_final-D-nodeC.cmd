executable = ./job_dagman_final-D-node.pl
output = job_dagman_final-D-nodeC.out
error = job_dagman_final-D-nodeC.err
log = job_dagman_final-D-nodeC.log
arguments = "D_C 0 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
queue
