universe   = scheduler
executable = x_sleep.pl
log = job_core_onexithold-false_sched.log
output = job_core_onexithold-false_sched.out
error = job_core_onexithold-false_sched.err
Notification = NEVER
arguments  = 3
on_exit_hold = (time() - QDate) < (2 )
queue

