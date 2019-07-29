executable   = job_dagman_depth_first-nodeB.pl
arguments    = $(nodename)
universe     = local
output       = $(job).out
error        = $(job).err
log          = job_dagman_depth_first.log
Notification = NEVER
queue
