executable   = job_dagman_usedagdir-node.pl
arguments    = $(nodename)
universe     = local
output       = job_dagman_usedagdir-$(nodename).out
error        = job_dagman_usedagdir-$(nodename).err
log          = job_dagman_usedagdir.log
Notification = NEVER
queue
