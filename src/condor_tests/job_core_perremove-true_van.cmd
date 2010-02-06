universe   = vanilla
executable = x_sleep.pl
log = job_core_perremove-true_van.log
output = job_core_perremove-true_van.out
error = job_core_perremove-true_van.err
periodic_remove = (time() - QDate) > 2
Notification = NEVER
arguments  = 3
queue

