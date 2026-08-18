executable           	= /bin/echo
arguments		= job_dagman_splice-cat.cmd $(cluster) OK
universe             	= local
log			= job_dagman_splice-cat.log
notification         	= NEVER
getenv = CONDOR*,PATH
output			= job_dagman_splice-cat.$(cluster).out
error			= job_dagman_splice-cat.err
queue

