universe   = java
executable = x_java_Sleep.class
log = job_core_perrelease-false_java.log
output = job_core_perrelease-false_java.out
error = job_core_perrelease-false_java.err
hold	= True
periodic_release = (CurrentTime - QDate) < 0
Notification = NEVER
arguments  = x_java_Sleep 10
queue

