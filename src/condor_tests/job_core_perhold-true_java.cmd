universe   = java
executable = x_java_Sleep.class
log = job_core_perhold-true_java.log
output = job_core_perhold-true_java.out
error = job_core_perhold-true_java.err
hold	= false
periodic_hold = (CurrentTime - QDate) > 5
Notification = NEVER
arguments  = x_java_Sleep 40
queue

