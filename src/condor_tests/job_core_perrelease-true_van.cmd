universe   = vanilla
executable = x_sleep.pl
log = job_core_perrelease-true_van.log
output = job_core_perrelease-true_van.out
error = job_core_perrelease-true_van.err
hold	= True
periodic_release = (CurrentTime - QDate) > 3
Notification = NEVER
arguments  = 6
queue

