executable           	= ./x_echostring.pl
arguments		= 1 2 3
universe             	= vanilla
log			= job_dagman_event_timestamp-node.log
notification         	= NEVER
getenv = CONDOR*,PATH
output			= job_dagman_event_timestamp-node.out
error			= job_dagman_event_timestamp-node.err
queue
