executable           	= /bin/echo
arguments		= job_dagman_subdag_multi_prohibit-SB-node.cmd $(nodename) OK
universe             	= scheduler
log			= job_dagman_subdag_multi_prohibit-SB.log
notification         	= NEVER
getenv               	= true
output			= job_dagman_subdag_multi_prohibit-B-node.$(cluster).$(process).out
error			= job_dagman_subdag_multi_prohibit-B-node.$(cluster).$(process).err
queue
