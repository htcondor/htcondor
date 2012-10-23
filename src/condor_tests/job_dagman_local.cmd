executable = /bin/echo
arguments = $(node) ran successfully
universe = local
output = job_dagman_local.$(cluster).out
log = job_dagman_local.$(cluster).log
error = job_dagman_local.$(cluster).err
queue
