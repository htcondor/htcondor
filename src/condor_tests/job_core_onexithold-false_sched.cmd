universe   = scheduler
executable = /bin/sleep
log = job_core_onexithold-false_sched.log
output = job_core_onexithold-false_sched.out
error = job_core_onexithold-false_sched.err
Notification = NEVER
arguments  = 3
on_exit_hold = (CurrentTime - QDate) < (2 )
queue

