universe = vanilla
executable = x_job_core_stream.pl
log = job_core_streamerr-false_van.log
output = job_core_streamerr-false_van.out
error = job_core_streamerr-false_van.err
when_to_transfer_output = ON_EXIT_OR_EVICT
stream_error = false
arguments = stderr 800000
getenv=false
Notification = NEVER
queue

