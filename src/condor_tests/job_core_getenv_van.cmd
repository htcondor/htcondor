universe = vanilla
executable = job_core_getenv.pl
log = job_core_getenv_van.log
output = job_core_getenv_van.out
error = job_core_getenv_van.err
arguments = failok
getenv=false
Notification = NEVER
queue
arguments = failnotok
getenv=true
queue

