universe   = scheduler
executable = /bin/sleep
log = job_core_perhold-true_sched.log
output = job_core_perhold-true_sched.out
error = job_core_perhold-true_sched.err
hold	= false
periodic_hold = (CurrentTime - QDate) > 5
Notification = NEVER
arguments  = 40
queue

