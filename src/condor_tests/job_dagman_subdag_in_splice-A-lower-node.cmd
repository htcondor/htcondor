executable           	= /bin/echo
arguments		= $(nodename) OK
universe             	= scheduler
log			= job_dagman_subdag_in_splice-A-lower-node.log
notification         	= NEVER
getenv = CONDOR*,PATH
output			= job_dagman_subdag_in_splice-A-lower-node.$(nodename).out
error			= job_dagman_subdag_in_splice-A-lower-node.err
queue

