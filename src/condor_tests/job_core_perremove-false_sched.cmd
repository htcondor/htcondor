universe   = scheduler
executable = x_sleep.pl
log = job_core_perremove-false_sched.log
output = job_core_perremove-false_sched.out
error = job_core_perremove-false_sched.err
Notification = NEVER
arguments  = 3
periodic_remove = (time() - QDate) < (0 )
queue

