universe   = vanilla
executable = ./x_job_filexfer_testjob.pl
log = job_filexfer_limitback_van.log
output = job_filexfer_limitback_van.out
error = job_filexfer_limitback_van.err
transfer_output_files = submit_filetrans_output14719f.txt,submit_filetrans_output14719g.txt
should_transfer_files = YES
when_to_transfer_output = ON_EXIT
Notification = NEVER
arguments = --job=14719 --onesetout 
queue

