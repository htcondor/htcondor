Executable           	= ./x_dagman_node-ret.pl
Universe             	= vanilla
log = job_dagman_prepost-node.log
Notification         	= NEVER
getenv = CONDOR*,PATH
output = job_dagman_prepost-node.out
error = job_dagman_prepost-node.err
Queue

