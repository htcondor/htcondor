executable   = ./job_dagman_log_path-node.pl
universe     = scheduler
output       = job_dagman_log_path.out
error        = job_dagman_log_path.err
# Note funky path to log file.
log          = ./job_dagman_log_path.log
Notification = NEVER
queue
