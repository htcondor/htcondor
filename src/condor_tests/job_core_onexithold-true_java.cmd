universe   = java
executable = x_java_Sleep.class
log = job_core_onexithold-true_java.log
output = job_core_onexithold-true_java.out
error = job_core_onexithold-true_java.err
on_exit_hold = (CurrentTime - QDate) < (2 * 60)
Notification = NEVER
arguments  = x_java_Sleep 3
queue

