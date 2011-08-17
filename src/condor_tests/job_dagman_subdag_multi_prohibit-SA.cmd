executable           	= /bin/echo
arguments		= job_dagman_subdag_multi_prohibit-SA-node.cmd $(nodename) OK
universe             	= scheduler
log			= job_dagman_subdag_multi_prohibit-SA.log
notification         	= NEVER
getenv               	= true
output			= job_dagman_subdag_multi_prohibit-A-node.$(cluster).$(process).out
error			= job_dagman_subdag_multi_prohibit-A-node.$(cluster).$(process).err
queue 2
