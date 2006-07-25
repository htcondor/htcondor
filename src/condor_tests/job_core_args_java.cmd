universe   = java
executable = x_java_CondorEcho.class
output = job_core_args_java.out
error = job_core_args_java.err
log = job_core_args_java.log
Notification = NEVER
arguments  = x_java_CondorEcho a \"& >/dev/null |echo -a ----- `foo` \n !@#$%^&*()><-_+=|
notification = never
queue

