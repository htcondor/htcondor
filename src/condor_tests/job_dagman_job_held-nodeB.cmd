Executable           	= ./job_dagman_job_held-node.pl
Universe             	= vanilla
Notification         	= NEVER
log			= job_dagman_job_held-node.log
output			= job_dagman_job_held-nodeA.out
error			= job_dagman_job_held-nodeA.err

# Force attempt to transfer a non-existant output file,
# so that this job keeps going on hold.
should_transfer_files	= YES
when_to_transfer_output = ON_EXIT
transfer_output_files	= this_file_does_not_exist
periodic_release	= true

Queue 3
