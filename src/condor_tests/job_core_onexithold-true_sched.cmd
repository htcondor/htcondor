universe   = scheduler
executable = x_sleep.pl
log = job_core_onexithold-true_sched.log
output = job_core_onexithold-true_sched.out
error = job_core_onexithold-true_sched.err
on_exit_hold = (CurrentTime - QDate) < (2 * 60)
Notification = NEVER
arguments  = 3
queue

