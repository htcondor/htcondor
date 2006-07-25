universe   = vanilla
executable = ./x_job_filexfer_testjob.pl
log = job_filexfer_whento-withinput_van.log
output = job_filexfer_whento-withinput_van.out
error = job_filexfer_whento-withinput_van.err
input = job_14889_dir/submit_filetrans_input14889.txt
transfer_input_files = job_14889_dir/submit_filetrans_input14889a.txt,job_14889_dir/submit_filetrans_input14889b.txt,job_14889_dir/submit_filetrans_input14889c.txt
should_transfer_files = YES
when_to_transfer_output = ON_EXIT
Notification = NEVER
arguments = --job=14889 --extrainput 
queue

