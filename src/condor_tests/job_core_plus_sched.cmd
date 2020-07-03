universe   = scheduler
executable = x_sleep.pl
log = job_core_plus_sched.log
output = job_core_plus_sched.out
error = job_core_plus_sched.err
hold = true
+foo		= "bar"
+bar		= "foo"
+last		= "first"
+done		= FALSE
Notification = NEVER
arguments  = 3600
queue

