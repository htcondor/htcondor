executable           	= /bin/echo
arguments		= job_dagman_subdag_multi_prohibit-B-node.cmd $(nodename) OK
universe             	= scheduler
log			= job_dagman_subdag_multi_prohibit-B-node.log
notification         	= NEVER
getenv               	= true
output			= job_dagman_subdag_multi_prohibit-B-node.$(nodename).out
error			= job_dagman_subdag_multi_prohibit-B-node.err
queue
