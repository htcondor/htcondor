universe   = scheduler
executable = ./x_general_server.pl
log = job_dagman_prepost-srvr.log
output = job_dagman_prepost-srvr.out
error = job_dagman_prepost-srvr.err
arguments = /tmp/prepostsock job_dagman_prepost-srvr-log.log
Notification = NEVER
queue

