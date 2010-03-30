executable           	= /bin/echo
arguments		= job_dagman_subdag_in_splice-A-lowerlower-node.cmd $(nodename) OK
universe             	= scheduler
log			= job_dagman_subdag_in_splice-A-lowerlower-node.log
notification         	= NEVER
getenv               	= true
output			= job_dagman_subdag_in_splice-A-lowerlower-node.$(nodename).out
error			= job_dagman_subdag_in_splice-A-lowerlower-node.err
queue

