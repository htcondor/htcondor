executable           	= /bin/echo
arguments		= job_dagman_subdag_multi_prohibit-SC-node.cmd $(nodename) OK
universe             	= scheduler
log			= job_dagman_subdag_multi_prohibit-SC.log
notification         	= NEVER
getenv               	= true
output			= job_dagman_subdag_multi_prohibit-C-node.$(cluster).$(process).out
error			= job_dagman_subdag_multi_prohibit-C-node.$(cluster).$(process).err
queue 2
