universe   = scheduler
executable = /bin/sleep
log = job_core_onexitrem-false_sched.log
output = job_core_onexitrem-false_sched.out
error = job_core_onexitrem-false_sched.err
Notification = NEVER
arguments  = 3
on_exit_remove = (CurrentTime - QDate) > (2 )
queue

