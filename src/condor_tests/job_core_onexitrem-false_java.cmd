universe   = java
executable = x_java_Sleep.class
log = job_core_onexitrem-false_java.log
output = job_core_onexitrem-false_java.out
error = job_core_onexitrem-false_java.err
Notification = NEVER
arguments  = x_java_Sleep 3
on_exit_remove = (CurrentTime - QDate) > (2 )
queue

