universe   = vanilla
executable = ./x_job_filexfer_testjob.pl
log = job_filexfer_whento-simpleinput_van.log
output = job_filexfer_whento-simpleinput_van.out
error = job_filexfer_whento-simpleinput_van.err
input = job_14874_dir/submit_filetrans_input14874.txt
should_transfer_files = YES
when_to_transfer_output = ON_EXIT
Notification = NEVER
arguments = --job=14874 --noextrainput 
queue

