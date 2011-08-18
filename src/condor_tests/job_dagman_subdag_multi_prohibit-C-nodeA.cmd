executable		= /bin/echo
arguments		= job_dagman_subdag_multi_prohibit-C-nodeA.cmd $(nodename) OK
universe		= scheduler
log				= job_dagman_subdag_multi_prohibit-C-nodeA.log
notification	= NEVER
getenv			= true
output			= job_dagman_subdag_multi_prohibit-C-nodeA.$(nodename).out
error			= job_dagman_subdag_multi_prohibit-C-nodeA.err
queue
