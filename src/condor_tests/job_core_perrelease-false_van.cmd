universe   = vanilla
executable = /bin/sleep
log = job_core_perrelease-false_van.log
output = job_core_perrelease-false_van.out
error = job_core_perrelease-false_van.err
hold	= True
periodic_release = (CurrentTime - QDate) < 0
Notification = NEVER
arguments  = 10
queue

