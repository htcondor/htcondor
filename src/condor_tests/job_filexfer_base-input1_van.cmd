universe   = vanilla
executable = ./x_job_filexfer_testjob.pl
log = job_filexfer_base-input1_van.log
output = job_filexfer_base-input1_van.out
error = job_filexfer_base-input1_van.err
input = job_14661_dir/submit_filetrans_input14661.txt
Notification = NEVER
arguments = --job=14661 --noextrainput --failok
queue

