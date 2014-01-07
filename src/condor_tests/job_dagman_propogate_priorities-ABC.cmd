executable = /bin/echo
arguments = DAG node $$([dagnodename]) has priority $$([jobprio])
error = $(jobname).err
output = $(jobname).out
log = job_dagman_propogate_priorities.log
queue
