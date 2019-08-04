executable = job_dagman_abort-final-A-nodeE.pl
output = job_dagman_abort-final-A-nodeE.out
error = job_dagman_abort-final-A-nodeE.err
arguments = "E 1 $(DAG_STATUS) $(FAILED_COUNT)"
universe = vanilla
notification = NEVER
queue
