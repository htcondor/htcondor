universe   = vanilla
executable = x_sleep.pl
log = job_core_onexithold-true_van.log
output = job_core_onexithold-true_van.out
error = job_core_onexithold-true_van.err
on_exit_hold = (CurrentTime - QDate) > 1
Notification = NEVER
arguments  = 3
queue

