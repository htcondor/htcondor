universe   = java
executable = ./x_java_Sleep.class
log = job_core_leaveinqueue-false_java.log
output = job_core_leaveinqueue-false_java.out
error = job_core_leaveinqueue-false_java.err
Notification = NEVER
arguments  = x_java_Sleep 3
leave_in_queue = (CurrentTime - QDate) < (2 )
queue

