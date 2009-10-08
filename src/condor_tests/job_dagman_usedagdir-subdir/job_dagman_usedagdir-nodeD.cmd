executable   = ./job_dagman_usedagdir-node.pl
arguments    = $(nodename)
universe     = scheduler
output       = job_dagman_usedagdir-$(nodename).out
error        = job_dagman_usedagdir-$(nodename).err
# Use different log file.
log          = job_dagman_usedagdir-nodeD.log
Notification = NEVER
queue
