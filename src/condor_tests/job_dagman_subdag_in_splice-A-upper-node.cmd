executable           	= /bin/echo
arguments		= $(nodename) OK
universe             	= scheduler
log			= job_dagman_subdag_in_splice-A-upper-node.log
notification         	= NEVER
getenv = CONDOR*,PATH
output			= job_dagman_subdag_in_splice-A-upper-node.$(nodename).out
error			= job_dagman_subdag_in_splice-A-upper-node.err
queue

