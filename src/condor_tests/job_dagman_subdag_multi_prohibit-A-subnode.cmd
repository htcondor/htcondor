executable		= /bin/echo
arguments		= job_dagman_subdag_multi_prohibit-A-subnode.cmd $(nodename) OK
universe		= scheduler
log				= job_dagman_subdag_multi_prohibit-A-subnode.log
notification	= NEVER
getenv			= true
output			= job_dagman_subdag_multi_prohibit-A-subnode.$(cluster).$(process).out
error			= job_dagman_subdag_multi_prohibit-A-subnode.$(cluster).$(process).err
queue 2
