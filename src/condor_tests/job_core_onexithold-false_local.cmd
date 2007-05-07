universe   = local
executable = x_sleep.pl
log = job_core_onexithold-false_local.log
output = job_core_onexithold-false_local.out
error = job_core_onexithold-false_local.err
Notification = NEVER
arguments  = 5
on_exit_hold = (CurrentTime - QDate) < (2)
queue

