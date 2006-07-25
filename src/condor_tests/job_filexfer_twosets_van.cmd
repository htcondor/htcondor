universe   = vanilla
executable = ./job_filexfer_testjob.pl
log = job_filexfer_twosets_van.log
output = job_filexfer_twosets_van.out
error = job_filexfer_twosets_van.err
should_transfer_files = YES
when_to_transfer_output = ON_EXIT
Notification = NEVER
arguments = --job=24899 --long 
queue

