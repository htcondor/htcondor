executable = job_dagman_abort-final-B-nodeE.pl
output = job_dagman_abort-final-B-nodeE.out
error = job_dagman_abort-final-B-nodeE.err
arguments = "E 0 $(DAG_STATUS) $(FAILED_COUNT)"
universe = vanilla
notification = NEVER
queue
