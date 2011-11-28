executable = ./job_dagman_final-A-node.pl
output = job_dagman_final-A-nodeB.out
error = job_dagman_final-A-nodeB.err
log = job_dagman_final-A-nodeB.log
arguments = "'OK done with B' 0 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
queue
