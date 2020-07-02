universe   = vanilla
executable = x_sleep.pl
log = job_core_plus_van.log
output = job_core_plus_van.out
error = job_core_plus_van.err
hold = true
+foo		= "bar"
+bar		= "foo"
+last		= "first"
+done		= FALSE
Notification = NEVER
arguments  = 3600
queue

