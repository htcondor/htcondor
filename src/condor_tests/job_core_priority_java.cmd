universe   = java
executable = x_java_CopyFilePlus.class
output = job_core_priority_java.out
error = job_core_priority_java.err
log = job_core_priority_java.log
should_transfer_files   = YES
when_to_transfer_output = ON_EXIT
transfer_input_files    = x_java_CopyFilePlus.class,job_core_priority_java.data
transfer_output_files   = job_core_priority_java.data
arguments = x_java_CopyFilePlus 1 job_core_priority_java.data
priority = -1
Notification = NEVER
queue
arguments = CopyFilePlus 8 job_core_priority_java.data
priority = -8
queue
arguments = CopyFilePlus 3 job_core_priority_java.data
priority = -3
queue
arguments = CopyFilePlus 10 job_core_priority_java.data
priority = -10
queue
arguments = CopyFilePlus 5 job_core_priority_java.data
priority = -5
queue
arguments = CopyFilePlus 2 job_core_priority_java.data
priority = -2
queue

