universe   = scheduler
executable = ./x_general_server.pl
log = job_dagman_maxprescripts.log
output = job_dagman_maxprescripts.out
error = job_dagman_maxprescripts.err
arguments = /tmp/maxpresock job_dagman_maxprescripts-srvr-log.log
Notification = NEVER
queue

