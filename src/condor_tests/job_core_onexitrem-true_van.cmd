universe   = vanilla
executable = x_sleep.pl
log = job_core_onexitrem-true_van.log
output = job_core_onexitrem-true_van.out
error = job_core_onexitrem-true_van.err
on_exit_remove = (CurrentTime - QDate) > 4200
Notification = NEVER
arguments  = 3
queue

