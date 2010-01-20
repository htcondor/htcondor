universe   = vanilla
executable = x_sleep.pl
log = job_core_shadow-lessthan-memlimit_van.log
error = job_core_shadow-lessthan-memlimit_van.err
output = job_core_shadow-lessthan-memlimit_van.out
kill_sig	= 3
should_transfer_files   = YES
when_to_transfer_output = ON_EXIT_OR_EVICT
Notification = NEVER
wantcorefile = false
arguments = 0
queue 1

