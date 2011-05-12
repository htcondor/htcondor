executable = /bin/sh
output = $(job).out
error = $(job).err
arguments = "-c ''echo ""C is done""; exit 0;''"
universe = scheduler
queue
