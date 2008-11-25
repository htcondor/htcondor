executable           	= /bin/echo
arguments		= job_dagman_parallel-A-node.cmd $(nodename) OK
universe             	= scheduler
log			= job_dagman_parallel-A-node.log
notification         	= NEVER
getenv               	= true
output			= job_dagman_parallel-A-node.$(nodename).out
error			= job_dagman_parallel-A-node.err
queue 3

