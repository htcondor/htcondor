executable           	= /bin/echo
arguments		= job_dagman_abort-final-A-node.cmd $(nodename) OK
universe             	= scheduler
notification         	= NEVER
getenv               	= true
output			= job_dagman_abort-final-A-node.$(nodename).out
error			= job_dagman_abort-final-A-node.err
queue

