universe   = vanilla
executable = x_copy_binary_file.pl
log = job_filexfer_basic_van.log
output = job_filexfer_basic_van.out
error = job_filexfer_basic_van.err
arguments  = job_filexfer_basic.data submit_transferfile_vanilla.txtdata
should_transfer_files   = YES
when_to_transfer_output = ON_EXIT
transfer_input_files    = job_filexfer_basic.data
transfer_output_files   = submit_transferfile_vanilla.txtdata
Notification = NEVER
queue

