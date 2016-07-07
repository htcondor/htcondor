executable = /bin/echo
arguments = DAG node $$([dagnodename]) has priority $$([jobprio])
error = $(jobname).err
output = $(jobname).out
queue
