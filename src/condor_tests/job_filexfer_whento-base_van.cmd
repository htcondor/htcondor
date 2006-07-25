universe   = vanilla
executable = ./x_job_filexfer_testjob.pl
log = job_filexfer_whento-base_van.log
output = job_filexfer_whento-base_van.out
error = job_filexfer_whento-base_van.err
should_transfer_files = YES
when_to_transfer_output = ON_EXIT
Notification = NEVER
arguments = --job=14860 --noinput 
queue

