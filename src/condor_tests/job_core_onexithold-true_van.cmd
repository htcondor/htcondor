universe   = vanilla
executable = /bin/sleep
log = job_core_onexithold-true_van.log
output = job_core_onexithold-true_van.out
error = job_core_onexithold-true_van.err
on_exit_hold = (CurrentTime - QDate) < (2 * 60)
Notification = NEVER
arguments  = 3
queue

