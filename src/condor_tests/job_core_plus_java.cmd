universe   = java
executable = x_java_Sleep.class
log = job_core_plus_java.log
output = job_core_plus_java.out
error = job_core_plus_java.err
+foo		= "bar"
+bar		= "foo"
+last		= "first"
+done		= FALSE
Notification = NEVER
arguments  = x_java_Sleep 2
queue

