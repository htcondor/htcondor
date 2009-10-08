universe   = java
executable = ./x_java_Sleep.class
log        = job_core_leaveinqueue-willtrigger_java.log
output = job_core_leaveinqueue-true_java.out
error = job_core_leaveinqueue-true_java.err
leave_in_queue = (CurrentTime - QDate) > (2 )
Notification = NEVER
arguments  = x_java_Sleep 3
queue

