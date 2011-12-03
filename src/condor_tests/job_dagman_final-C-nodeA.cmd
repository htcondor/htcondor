executable = ./job_dagman_final-C-nodeA.pl
output = job_dagman_final-C-nodeA.out
error = job_dagman_final-C-nodeA.err
log = job_dagman_final-C-nodeA.log
arguments = "$(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
notification = NEVER
queue
