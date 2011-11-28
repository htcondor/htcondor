executable = ./job_dagman_final-A-node.pl
output = job_dagman_final-A-nodeC.out
error = job_dagman_final-A-nodeC.err
log = job_dagman_final-A-nodeC.log
arguments = "'$(msg)' 0 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
queue
