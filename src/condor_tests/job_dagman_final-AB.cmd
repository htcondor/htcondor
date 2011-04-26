executable = /bin/sh
output = $(job).out
error = $(job).err
log = submit-AB.log
arguments = -c \"echo 'B is done'; exit 0\"
universe = scheduler
queue
