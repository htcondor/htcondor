universe   = vanilla
executable = x_sleep.pl
# Avoid having the starter wait around for a reconnect
job_lease_duration = 0
output = job_schedd_restart-running-ok.out
error = job_schedd_restart-running-ok.err
log = job_schedd_restart-running-ok.log
Notification = NEVER
arguments = 0
queue 5
