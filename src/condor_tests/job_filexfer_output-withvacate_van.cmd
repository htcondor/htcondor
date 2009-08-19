universe   = vanilla
# Insure that we only run once, that we don't restart after vacation
requirements = isUndefined(My.NumShadowStarts) || (My.NumShadowStarts < 2)
executable = ./x_job_filexfer_testjob.pl
log = job_filexfer_output-withvacate_van.log
output = job_filexfer_output-withvacate_van.out
error = job_filexfer_output-withvacate_van.err
should_transfer_files = YES
when_to_transfer_output = ON_EXIT_OR_EVICT
transfer_output_files = submit_filetrans_output14821e.txt,submit_filetrans_output14821f.txt,submit_filetrans_output14821g.txt
Notification = NEVER
arguments = --job=14821 --forever 
queue

