universe   = scheduler
executable = /bin/sleep
log = job_core_perremove-true_sched.log
output = job_core_perremove-true_sched.out
error = job_core_perremove-true_sched.err
periodic_remove = (CurrentTime - QDate) > 2
Notification = NEVER
arguments  = 3
queue

