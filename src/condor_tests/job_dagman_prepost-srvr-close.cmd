universe   = scheduler
executable = ./x_general_client.pl
log = job_dagman_prepost-srvr-close.log
output = job_dagman_prepost-srvr-close.out
error = job_dagman_prepost-srvr-close.err
Notification = NEVER
arguments = /tmp/prepostsock quit
queue

