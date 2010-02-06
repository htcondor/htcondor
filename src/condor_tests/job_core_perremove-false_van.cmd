universe   = vanilla
executable = x_sleep.pl
log = job_core_perremove-false_van.log
output = job_core_perremove-false_van.out
error = job_core_perremove-false_van.err
Notification = NEVER
arguments  = 3
periodic_remove = (time() - QDate) < (0 )
queue

