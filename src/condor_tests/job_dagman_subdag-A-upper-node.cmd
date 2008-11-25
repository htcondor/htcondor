executable           	= /bin/echo
arguments		= job_dagman_subdag-A-upper-node.cmd $(nodename) OK
universe             	= scheduler
log			= job_dagman_subdag-A-upper-node.log
notification         	= NEVER
getenv               	= true
output			= job_dagman_subdag-A-upper-node.$(nodename).out
error			= job_dagman_subdag-A-upper-node.err
queue

