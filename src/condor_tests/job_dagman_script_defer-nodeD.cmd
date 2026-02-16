executable   = ./job_dagman_script_defer-nodeD.pl
arguments    = $(DAGManJobId)
universe     = scheduler
output       = job_dagman_script_defer-nodeD.out
error        = job_dagman_script_defer-nodeD.err
log          = job_dagman_script_defer-nodeD.log
getenv = CONDOR*,PATH
Notification = NEVER
queue
