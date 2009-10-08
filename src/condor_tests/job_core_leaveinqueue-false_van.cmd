universe   = vanilla
executable = ./x_sleep.pl
log = job_core_leaveinqueue-false_van.log
output = job_core_leaveinqueue-false_van.out
error = job_core_leaveinqueue-false_van.err
Notification = NEVER
arguments  = 3
leave_in_queue = (CurrentTime - QDate) < (2 )
queue

