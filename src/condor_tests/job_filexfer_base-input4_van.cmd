universe   = vanilla
executable = ./x_job_filexfer_testjob.pl
log = job_filexfer_base-input4_van.log
output = job_filexfer_base-input4_van.out
error = job_filexfer_base-input4_van.err
input = job_14675_dir/submit_filetrans_input14675.txt
transfer_input_files = job_14675_dir/submit_filetrans_input14675a.txt,job_14675_dir/submit_filetrans_input14675b.txt,job_14675_dir/submit_filetrans_input14675c.txt
Notification = NEVER
arguments = --job=14675 --extrainput 
queue

