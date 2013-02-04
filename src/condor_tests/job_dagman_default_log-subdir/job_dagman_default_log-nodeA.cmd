executable   = ./job_dagman_default_log-nodeA.pl
universe     = scheduler
output       = job_dagman_default_log-nodeA.out
error        = job_dagman_default_log-nodeA.err
# Adding this to make sure that a macro in the log file name
# doesn't goof things up (even in recovery mode) when we're using the
# workflow log.
log          = $(LogFile)
Notification = NEVER
queue
