executable           	= /bin/echo
arguments		= job_dagman_pre_subdag-A-node.cmd $(nodename) OK
universe             	= scheduler
notification         	= NEVER
getenv = CONDOR*,PATH
output			= job_dagman_pre_subdag-A-node.$(nodename).out
error			= job_dagman_pre_subdag-A-node.err
queue

