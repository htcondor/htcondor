universe   = scheduler
executable = job_core_priority.pl
output = job_core_priority_sched.out
error = job_core_priority_sched.err
log = job_core_priority_sched.log
should_transfer_files   = YES
when_to_transfer_output = ON_EXIT
transfer_input_files    = job_core_priority_sched.data
transfer_output_files   = job_core_priority_sched.data
arguments = 1 job_core_priority_sched.data
priority = -1
Notification = NEVER
queue
arguments = 8 job_core_priority_sched.data
priority = -8
queue
arguments = 3 job_core_priority_sched.data
priority = -3
queue
arguments = 10 job_core_priority_sched.data
priority = -10
queue
arguments = 5 job_core_priority_sched.data
priority = -5
queue
arguments = 2 job_core_priority_sched.data
priority = -2
queue

