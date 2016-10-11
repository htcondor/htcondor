universe = scheduler
executable = ./job_dagman_rm-wait.pl
arguments = "job_dagman_rm-NodeZ-job.$(CLUSTER)"
log = job_dagman_rm-nodeZ.log
output = job_dagman_rm-nodeZ.out
error = job_dagman_rm-nodeZ.err
notification = NEVER
queue
