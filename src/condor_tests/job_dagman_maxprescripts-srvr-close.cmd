universe   = scheduler
executable = ./x_general_client.pl
log = job_dagman_maxprescripts-srvr-close.log
output = job_dagman_maxprescripts-srvr-close.out
error = job_dagman_maxprescripts-srvr-close.err
Notification = NEVER
arguments = /tmp/maxpresock quit
queue

