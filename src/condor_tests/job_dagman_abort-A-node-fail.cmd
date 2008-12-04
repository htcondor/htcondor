executable           	= ./x_dagman_retry-monitor.pl
arguments		= 5
universe             	= scheduler
log			= job_dagman_abort-A-node.log
notification         	= NEVER
getenv               	= true
output			= job_dagman_abort-A-node.$(nodename).out
error			= job_dagman_abort-A-node.err
queue

