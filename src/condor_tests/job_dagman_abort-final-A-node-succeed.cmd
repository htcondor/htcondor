executable           	= ./x_echostring.pl
arguments		= job_dagman_abort-final-A-node.cmd $(nodename) OK
# Universe changed from scheduler -- see gittrac #4394
universe             	= vanilla
notification         	= NEVER
getenv               	= true
output			= job_dagman_abort-final-A-node.$(nodename).out
error			= job_dagman_abort-final-A-node.err
queue

