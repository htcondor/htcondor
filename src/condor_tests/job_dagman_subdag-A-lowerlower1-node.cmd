executable           	= /bin/echo
arguments		= job_dagman_subdag-A-lowerlower1-node.cmd $(nodename) OK
universe             	= scheduler
log			= job_dagman_subdag-A-lowerlower1-node.log
notification         	= NEVER
getenv               	= true
output			= job_dagman_subdag-A-lowerlower1-node.$(nodename).out
error			= job_dagman_subdag-A-lowerlower1-node.err
queue

