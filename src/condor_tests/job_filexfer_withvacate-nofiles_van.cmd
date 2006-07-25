universe   = vanilla
executable = ./x_job_filexfer_testjob.pl
log = job_filexfer_withvacate-nofiles_van.log
output = job_filexfer_withvacate-nofiles_van.out
error = job_filexfer_withvacate-nofiles_van.err
transfer_output_files = submit_filetrans_output14908e.txt,submit_filetrans_output14908f.txt,submit_filetrans_output14908g.txt
should_transfer_files = YES
when_to_transfer_output = ON_EXIT
Notification = NEVER
arguments = --job=14908 --long 
queue

