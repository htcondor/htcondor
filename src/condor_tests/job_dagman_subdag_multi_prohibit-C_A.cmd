executable           	= /bin/echo
arguments		= job_dagman_subdag_multi_prohibit-C-node.cmd $(nodename) OK
universe             	= scheduler
log			= job_dagman_subdag_multi_prohibit-C-node.log
notification         	= NEVER
getenv               	= true
output			= job_dagman_subdag_multi_prohibit-C-node.$(nodename).out
error			= job_dagman_subdag_multi_prohibit-C-node.err
queue
