universe   = vanilla
executable = x_sleep.pl
log = job_core_perhold-false_van.log
output = job_core_perhold-false_van.out
error = job_core_perhold-false_van.err
hold	= false
periodic_hold = (CurrentTime - QDate) < 0
Notification = NEVER
arguments  = 4
queue

