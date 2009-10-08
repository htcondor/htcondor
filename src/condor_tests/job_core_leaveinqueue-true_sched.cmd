universe   = scheduler
executable = ./x_sleep.pl
log = job_core_leaveinqueue-true_sched.log
output = job_core_leaveinqueue-true_sched.out
error = job_core_leaveinqueue-true_sched.err
leave_in_queue = (CurrentTime - QDate) > (2 )
Notification = NEVER
arguments  = 3
queue

