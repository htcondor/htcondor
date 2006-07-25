universe   = vanilla
executable = x_sleep.pl
log = job_core_perhold-true_van.log
output = job_core_perhold-true_van.out
error = job_core_perhold-true_van.err
hold	= false
#periodic_hold = (CurrentTime - QDate) > 0
periodic_hold = JobStatus == 2
Notification = NEVER
arguments  = 40
queue

