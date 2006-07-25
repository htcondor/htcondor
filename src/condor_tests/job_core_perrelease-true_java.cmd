universe   = java
executable = x_java_Sleep.class
log = job_core_perrelease-true_java.log
output = job_core_perrelease-true_java.out
error = job_core_perrelease-true_java.err
hold	= True
periodic_release = (CurrentTime - QDate) > 3
Notification = NEVER
arguments  = x_java_Sleep 6
queue

