universe   = vanilla
executable = job_core_priority2.pl
output = job_core_priority_van.out
error = job_core_priority_van.err
log = job_core_priority_van.log
#should_transfer_files   = YES
#when_to_transfer_output = ON_EXIT
arguments = 1 
priority = -1
Notification = NEVER
queue
arguments = 8 
priority = -8
queue
arguments = 3 
priority = -3
queue
arguments = 10 
priority = -10
queue
arguments = 5 
priority = -5
queue
arguments = 2 
priority = -2
queue

