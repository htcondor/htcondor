universe   = scheduler
executable = x_sleep.pl
log = job_core_perhold-true_sched.log
output = job_core_perhold-true_sched.out
error = job_core_perhold-true_sched.err
hold	= false
periodic_hold = JobStatus == 2
Notification = NEVER
arguments  = 100
queue

