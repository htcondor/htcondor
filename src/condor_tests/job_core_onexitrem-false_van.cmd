universe   = vanilla
executable = x_sleep.pl
log = job_core_onexitrem-false_van.log
output = job_core_onexitrem-false_van.out
error = job_core_onexitrem-false_van.err
Notification = NEVER
arguments  = 3
on_exit_remove = (time() - QDate) > (2 )
queue

