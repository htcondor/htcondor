universe   = java
executable = x_java_Sleep.class
log = job_core_perremove-true_java.log
output = job_core_perremove-true_java.out
error = job_core_perremove-true_java.err
periodic_remove = (CurrentTime - QDate) > 2
Notification = NEVER
arguments  = x_java_Sleep 3
queue

