executable = ./job_dagman_final-A-node.pl
output = job_dagman_final-A-nodeA.out
error = job_dagman_final-A-nodeA.err
log = job_dagman_final-A-nodeA.log
arguments = "'OK done with A_A' 0 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
queue
