universe   = vanilla
executable = /bin/sleep
log = job_core_leaveinqueue-true_van.log
output = job_core_leaveinqueue-true_van.out
error = job_core_leaveinqueue-true_van.err
leave_in_queue = (CurrentTime - QDate) > (2 )
Notification = NEVER
arguments  = 3
queue

