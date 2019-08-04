executable   = $CHOICE(IsWindows,/bin/echo,appendmsg)
arguments    = OK
universe     = local
output       = $(job).out
error        = $(job).err
log          = job_dagman_depth_first.log
Notification = NEVER
queue
