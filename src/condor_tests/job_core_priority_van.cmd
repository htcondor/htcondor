universe   = vanilla
executable = job_core_priority.pl
output = job_core_priority_van.out
error = job_core_priority_van.err
log = job_core_priority_van.log
should_transfer_files   = YES
when_to_transfer_output = ON_EXIT
transfer_input_files    = job_core_priority_van.data
transfer_output_files   = job_core_priority_van.data
arguments = 1 job_core_priority_van.data
priority = -1
Notification = NEVER
queue
arguments = 8 job_core_priority_van.data
priority = -8
queue
arguments = 3 job_core_priority_van.data
priority = -3
queue
arguments = 10 job_core_priority_van.data
priority = -10
queue
arguments = 5 job_core_priority_van.data
priority = -5
queue
arguments = 2 job_core_priority_van.data
priority = -2
queue

