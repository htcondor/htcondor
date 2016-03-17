executable = /bin/echo
arguments = $(node) ran successfully
universe = local
output = job_dagman_local.$(cluster).out
error = job_dagman_local.$(cluster).err
# Note no log file specified!
queue
