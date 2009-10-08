universe   = java
executable = ./x_java_Sleep.class
log = job_core_perhold-false_java.log
output = job_core_perhold-false_java.out
error = job_core_perhold-false_java.err
hold	= false
periodic_hold = (CurrentTime - QDate) < 0
Notification = NEVER
arguments  = x_java_Sleep 4
queue

