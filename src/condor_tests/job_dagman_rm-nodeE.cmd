universe = scheduler
executable = ./job_dagman_rm-nodeE.pl
arguments = "job_dagman_rm-NodeE-job $(DAGManJobId)"
log = job_dagman_rm-nodeE.log
output = job_dagman_rm-nodeE.out
error = job_dagman_rm-nodeE.err
notification = NEVER
queue
