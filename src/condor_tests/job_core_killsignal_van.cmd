universe   = vanilla
executable = x_trapsig.exe
log = job_core_killsignal_van.log
error = job_core_killsignal_van.err
output = job_core_killsignal_van.out
kill_sig	= 3
should_transfer_files   = YES
when_to_transfer_output = ON_EXIT_OR_EVICT
Notification = NEVER
wantcorefile = false
queue 1

