universe   = scheduler
executable = x_trapsig.exe
log = job_core_rmkillsig_sched.log
error = job_core_rmkillsig_sched.err
output = job_core_rmkillsig_sched.out
remove_kill_sig	= 4
should_transfer_files   = YES
when_to_transfer_output = ON_EXIT_OR_EVICT
Notification = NEVER
wantcorefile = false
queue 1

