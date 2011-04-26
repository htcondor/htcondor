executable = /bin/sh
output = $(job).out
error = $(job).err
log = submit-BB.log
arguments = -c \"echo 'B is done'; exit 1\"
universe = scheduler
queue
