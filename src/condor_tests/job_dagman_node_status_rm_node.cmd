executable = job_dagman_node_status_rm_node.pl
output = job_dagman_node_status_rm_node.out
error = job_dagman_node_status_rm_node.err
log = job_dagman_node_status_rm_node.log
arguments =  "A $(DAGManJobId)"
universe = scheduler
getenv = CONDOR*,PATH
notification = NEVER
queue
