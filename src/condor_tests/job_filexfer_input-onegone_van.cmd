universe   = vanilla
executable = ./x_job_filexfer_testjob.pl
log = job_filexfer_input-onegone_van.log
output = job_filexfer_input-onegone_van.out
error = job_filexfer_input-onegone_van.err
input = job_14711_dir/submit_filetrans_input14711.txt
transfer_input_files = job_14711_dir/submit_filetrans_input14711a.txt,job_14711_dir/submit_filetrans_input14711b.txt,job_14711_dir/submit_filetrans_input14711c.txt
filetrans_input6492c.txt
should_transfer_files = YES
when_to_transfer_output = ON_EXIT
Notification = NEVER
arguments = --job=14711 --extrainput 
queue

