executable           	= /bin/echo
arguments		= job_dagman_abort-A-node.cmd $(nodename) OK
universe             	= scheduler
log			= job_dagman_abort-A-node.log
notification         	= NEVER
getenv               	= true
output			= job_dagman_abort-A-node.$(nodename).out
error			= job_dagman_abort-A-node.err
queue

