executable = job_dagman_condor_restart-nodeA.pl
universe = local
getenv = CONDOR*,PATH
output = job_dagman_condor_restart-nodeA.out
error = job_dagman_condor_restart-nodeA.err
log = job_dagman_condor_restart-nodeA.log
notification = NEVER
queue
