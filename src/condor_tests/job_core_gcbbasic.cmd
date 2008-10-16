universe   = vanilla
executable = x_sleep.pl
log = job_core_gcbbasic.log
output = job_core_gcbbasic.out
error = job_core_gcbbasic.err
Notification = NEVER
requirements = (TARGET.Arch == "X86_64")
should_transfer_files = YES
when_to_transfer_output = ON_EXIT
arguments  = 5
queue

