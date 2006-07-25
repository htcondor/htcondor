universe   = java
executable = x_java_Sleep.class
log = job_core_perremove-false_java.log
output = job_core_perremove-false_java.out
error = job_core_perremove-false_java.err
Notification = NEVER
arguments  = x_java_Sleep 3
periodic_remove = (CurrentTime - QDate) < (0 )
queue

