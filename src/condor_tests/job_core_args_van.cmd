universe   = vanilla
executable = job_core_args.pl
output = job_core_args_van.out
error = job_core_args_van.err
log = job_core_args_van.log
Notification = NEVER
arguments  = a \'| \?- \\+ \"& >/dev/null |echo -a ----- `foo` \n !@#$%^&*()><-_+=|
notification = never
queue

