universe = vanilla
executable = x_job_core_stream.pl
log = job_core_streamerr-true_van.log
output = job_core_streamerr-true_van.out
error = job_core_streamerr-true_van.err
when_to_transfer_output = ON_EXIT_OR_EVICT
stream_error = true
arguments = stderr 80000
getenv=false
Notification = NEVER
queue

