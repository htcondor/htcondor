universe   = scheduler
executable = /bin/sleep
log = job_core_perhold-false_sched.log
output = job_core_perhold-false_sched.out
error = job_core_perhold-false_sched.err
hold	= false
periodic_hold = (CurrentTime - QDate) < 0
Notification = NEVER
arguments  = 4
queue

