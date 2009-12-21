executable   = ./job_dagman_script_args-node.pl
arguments    = $(nodename)
universe     = scheduler
output       = job_dagman_script_args-$(nodename).out
error        = job_dagman_script_args-$(nodename).err
log          = job_dagman_script_args.log
Notification = NEVER
queue
