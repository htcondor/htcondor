universe   = java
executable = x_java_Sleep.class
log = job_core_onexitrem-true_java.log
output = job_core_onexitrem-true_java.out
error = job_core_onexitrem-true_java.err
on_exit_remove = (CurrentTime - QDate) > 60
Notification = NEVER
arguments  = x_java_Sleep 3
queue

