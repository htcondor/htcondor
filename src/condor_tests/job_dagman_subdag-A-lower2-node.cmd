executable           	= /bin/echo
arguments		= job_dagman_subdag-A-lower2-node.cmd $(nodename) OK
universe             	= scheduler
log			= job_dagman_subdag-A-lower2-node.log
notification         	= NEVER
getenv               	= true
output			= job_dagman_subdag-A-lower2-node.$(nodename).out
error			= job_dagman_subdag-A-lower2-node.err
queue

