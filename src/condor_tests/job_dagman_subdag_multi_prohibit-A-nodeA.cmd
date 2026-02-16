executable		= /bin/echo
arguments		= job_dagman_subdag_multi_prohibit-A-nodeA.cmd $(nodename) OK
universe		= local
log				= job_dagman_subdag_multi_prohibit-A-nodeA.log
notification	= NEVER
getenv = CONDOR*,PATH
output			= job_dagman_subdag_multi_prohibit-A-nodeA.$(nodename).out
error			= job_dagman_subdag_multi_prohibit-A-nodeA.err
queue
