executable = ./job_dagman_final-B-node.pl
output = job_dagman_final-B-nodeB.out
error = job_dagman_final-B-nodeB.err
log = job_dagman_final-B-nodeBlog
arguments = "'FAILED done with B_B' 1 $(DAG_STATUS) $(FAILED_COUNT)"
universe = scheduler
notification = NEVER
queue
