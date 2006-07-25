universe   = scheduler
executable = x_sleep.pl
log = job_core_perrelease-false_sched.log
output = job_core_perrelease-false_sched.out
error = job_core_perrelease-false_sched.err
hold	= True
periodic_release = (CurrentTime - QDate) < 0
Notification = NEVER
arguments  = 10
queue

