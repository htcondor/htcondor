universe   = local
executable = x_sleep.pl
log = job_core_onexitrem-false_local.log
output = job_core_onexitrem-false_local.out
error = job_core_onexitrem-false_local.err
Notification = NEVER
arguments  = 10
on_exit_remove = (CurrentTime - QDate) > 5000
queue

