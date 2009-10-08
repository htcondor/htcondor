executable   = ./job_dagman_log_path-node.pl
universe     = scheduler
output       = job_dagman_log_path.out
error        = job_dagman_log_path.err
# Log file is a symlink to job_dagman_log_path.log.
log          = job_dagman_log_pathC.log
Notification = NEVER
queue
