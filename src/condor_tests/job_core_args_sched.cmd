universe   = scheduler
executable = job_core_args.pl
output = job_core_args_sched.out
error = job_core_args_sched.err
log = job_core_args_sched.log
Notification = NEVER
arguments  = a \'| \?- \\+ \"& >/dev/null |echo -a ----- `foo` \n !@#$%^&*()><-_+=|
notification = never
queue

