universe   = scheduler
executable = job_core_queue.pl
transfer_input_files = x_general_client.pl
log = job_core_queue_sched.log
error = job_core_queue_sched.err
output = ./job_core_queue_sched.out
Notification = NEVER
arguments = hello
queue 40

