executable = job_negotiator_restart-nodeAC.pl
arguments = $(nodename)
universe = scheduler
getenv = true
output = job_negotiator_restart-node$(nodename).out
error = job_negotiator_restart-node$(nodename).err
log = job_negotiator_restart-nodeAC.log
notification = NEVER
queue
