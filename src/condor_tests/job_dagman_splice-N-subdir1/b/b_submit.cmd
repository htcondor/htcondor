executable   = /bin/echo
arguments    = OK
universe     = scheduler
output       = job_dagman_splice-N-subdir1/b/$(job).out
error        = job_dagman_splice-N-subdir1/b/$(job).err
log          = job_dagman_splice-N-subdir1/b/b_submit.log
Notification = NEVER
queue
