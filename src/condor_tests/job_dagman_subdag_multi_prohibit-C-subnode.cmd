executable		= /bin/echo
arguments		= job_dagman_subdag_multi_prohibit-C-subnode.cmd $(nodename) OK
universe		= scheduler
log				= job_dagman_subdag_multi_prohibit-C-subnode.log
notification	= NEVER
getenv			= true
output			= job_dagman_subdag_multi_prohibit-C-subnode.$(cluster).$(process).out
error			= job_dagman_subdag_multi_prohibit-C-subnode.$(cluster).$(process).err
queue 2
