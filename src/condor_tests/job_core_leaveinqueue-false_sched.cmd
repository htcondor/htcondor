universe   = scheduler
executable = /bin/sleep
log = job_core_leaveinqueue-false_sched.log
output = job_core_leaveinqueue-false_sched.out
error = job_core_leaveinqueue-false_sched.err
Notification = NEVER
arguments  = 3
leave_in_queue = (CurrentTime - QDate) < (2 )
queue

