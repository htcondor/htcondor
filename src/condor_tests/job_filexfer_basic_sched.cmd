universe   = scheduler
executable =  x_copy_binary_file.pl
log = job_filexfer_basic_sched.log
output = job_filexfer_basic_sched.out
error = job_filexfer_basic_sched.err
arguments  = job_filexfer_basic.data submit_transferfile_scheduler.txtdata
should_transfer_files   = YES
when_to_transfer_output = ON_EXIT
transfer_input_files    = job_filexfer_basic.data
transfer_output_files   = submit_transferfile_scheduler.txtdata
Notification = NEVER
queue

