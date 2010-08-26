executable   = ./job_dagman_job_hold-node.pl
universe     = vanilla
should_transfer_files	= yes
when_to_transfer_output = ON_EXIT
# We want this to fail for the test...
transfer_output_files	= file_does_not_exist
periodic_release	= true
log          = job_dagman_job_hold-node.log
Notification = NEVER
#queue
queue 3
