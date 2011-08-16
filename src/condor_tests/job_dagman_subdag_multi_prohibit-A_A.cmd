executable           	= /bin/echo
arguments		= job_dagman_subdag_multi_prohibit-A-node.cmd $(nodename) OK
universe             	= scheduler
log			= job_dagman_subdag_multi_prohibit-A-node.log
notification         	= NEVER
getenv               	= true
output			= job_dagman_subdag_multi_prohibit-A-node.$(nodename).out
error			= job_dagman_subdag_multi_prohibit-A-node.err
queue
