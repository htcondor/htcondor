universe = vanilla
executable = x_job_filexfer_stream.pl
log = job_filexfer_streamerr_van.log
output = job_filexfer_streamerr_van.out
error = job_filexfer_streamerr_van.err
when_to_transfer_output = ON_EXIT_OR_EVICT
stream_error = true
arguments = stderr 100 0
getenv=false
Notification = NEVER
queue

