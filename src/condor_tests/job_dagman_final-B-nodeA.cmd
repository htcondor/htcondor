executable = ./job_dagman_final-B-node.pl
output = job_dagman_final-B-nodeA.out
error = job_dagman_final-B-nodeA.err
log = job_dagman_final-B-nodeA.log
arguments = "'OK done with B_A' 0 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
queue
