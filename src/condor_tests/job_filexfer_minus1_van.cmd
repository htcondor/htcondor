universe   = vanilla
executable = ./x_job_filexfer_testjob.pl
log = job_filexfer_minus1_van.log
output = job_filexfer_minus1_van.out
error = job_filexfer_minus1_van.err
transfer_output_files = submit_filetrans_output14768d.txt,submit_filetrans_output14768e.txt,submit_filetrans_output14768f.txt,submit_filetrans_output14768g.txt
should_transfer_files = YES
when_to_transfer_output = ON_EXIT
Notification = NEVER
arguments = --job=14768 --onesetout 
queue

