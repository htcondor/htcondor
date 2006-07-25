universe   = vanilla
executable = ./x_job_filexfer_testjob.pl
log = job_filexfer_trans-excut-true_van.log
output = job_filexfer_trans-excut-true_van.out
error = job_filexfer_trans-excut-true_van.err
input	= submit_filetrans_input.txt
transfer_input = false
transfer_error = false
transfer_output = false
transfer_executable = true
Notification = NEVER
arguments  = --noextrainput
queue

