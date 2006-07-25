universe = java
executable =  x_java_CondorEcho.class
initialdir = initd
log = job_core_initialdir_java.log
output = job_core_initialdir_java.out
error = job_core_initialdir_java.err
getenv = true
arguments = x_java_CondorEcho 3 
Notification = NEVER
queue


