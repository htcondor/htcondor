universe   = scheduler
executable = ./x_general_client.pl
log = job_dagman_maxpostscripts_srvr_close.log
output = job_dagman_maxpostscripts_srvr_close.out
error = job_dagman_maxpostscripts_srvr_close.err
Notification = NEVER
arguments = /tmp/maxpostsock quit
queue

