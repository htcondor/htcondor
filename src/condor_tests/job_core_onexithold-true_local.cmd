universe   = local
executable = x_sleep.pl
log = job_core_onexithold-true_local.log
output = job_core_onexithold-true_local.out
error = job_core_onexithold-true_local.err
on_exit_hold = (CurrentTime - QDate) > 1
Notification = NEVER
arguments  = 5
queue

