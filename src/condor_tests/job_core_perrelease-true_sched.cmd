universe   = scheduler
executable = /bin/sleep
log = job_core_perrelease-true_sched.log
output = job_core_perrelease-true_sched.out
error = job_core_perrelease-true_sched.err
hold	= True
periodic_release = (CurrentTime - QDate) > 3
Notification = NEVER
arguments  = 6
queue

