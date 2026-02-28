executable		= /bin/echo
arguments		= job_dagman_subdag_multi_prohibit-B-subnode.cmd $(nodename) OK
universe		= scheduler
log				= job_dagman_subdag_multi_prohibit-B-subnode.log
notification	= NEVER
getenv = CONDOR*,PATH
output			= job_dagman_subdag_multi_prohibit-B-subnode.$(cluster).$(process).out
error			= job_dagman_subdag_multi_prohibit-B-subnode.$(cluster).$(process).err
queue
