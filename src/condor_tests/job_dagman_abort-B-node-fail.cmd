executable           	= ./x_dagman_retry-monitor.pl
arguments		= 5
universe             	= scheduler
notification         	= NEVER
getenv               	= true
output			= job_dagman_abort-B-node.$(nodename).out
error			= job_dagman_abort-B-node.err
queue

