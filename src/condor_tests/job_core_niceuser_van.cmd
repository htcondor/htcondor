universe   = vanilla
executable = x_do_niceuser.pl
output = job_core_niceuser_van.out
error = job_core_niceuser_van.err
log = job_core_niceuser_van.log
should_transfer_files   = YES
when_to_transfer_output = ON_EXIT
transfer_input_files    = job_core_niceuser_van.data,CondorTest.pm,Condor.pm,CondorPersonal.pm
transfer_output_files   = job_core_niceuser_van.data
arguments = 2 job_core_niceuser_van.data
nice_user = true
Notification = NEVER
queue
nice_user = false
arguments = 1 job_core_niceuser_van.data
queue
