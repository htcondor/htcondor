universe = vanilla
executable = x_job_core_stream.pl
log = job_core_streamout_van.log
output = job_core_streamout_van.out
error = job_core_streamout_van.err
when_to_transfer_output = ON_EXIT_OR_EVICT
stream_output = true
arguments = stdout 280000
getenv=false
Notification = NEVER
queue

