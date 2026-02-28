executable   = ./job_dagman_rescue_recov-node_hold-release.pl
arguments    = $(nodename) $(DAGManJobId)
universe     = scheduler
output       = job_dagman_rescue_recov-node_hold-release.out
error        = job_dagman_rescue_recov-node_hold-release.err
getenv = CONDOR*,PATH
# Note: this submit file uses the DAGMan default log file feature.
Notification = NEVER
queue
