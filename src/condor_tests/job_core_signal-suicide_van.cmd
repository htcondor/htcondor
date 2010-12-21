universe   = vanilla
executable = x_job_signal-suicide.pl
log = job_core_signal-suicide_van.log
error = job_core_signal-suicide_van.err
output = job_core_signal-suicide_van.out
should_transfer_files = YES
Notification = NEVER
arguments  = $(Process)
wantcorefile = false
queue 15

