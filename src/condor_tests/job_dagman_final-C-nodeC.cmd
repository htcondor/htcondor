executable = ./job_dagman_final-C-node.pl
output = job_dagman_final-C-nodeC.out
error = job_dagman_final-C-nodeC.err
log = job_dagman_final-C-nodeC.log
arguments = "C_C 1 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
queue
