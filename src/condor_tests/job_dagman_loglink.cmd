executable		= /bin/echo
arguments		= OK $(nodename)
universe		= scheduler
log				= job_dagman_loglink.node.log
notification	= NEVER
output			= job_dagman_loglink_$(nodename).out
error			= job_dagman_loglink_$(nodename).err
queue
