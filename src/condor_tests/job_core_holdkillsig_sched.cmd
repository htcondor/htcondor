universe   = scheduler
executable = ./x_trapsig.exe
log = job_core_holdkillsig_sched.log
error = job_core_holdkillsig_sched.err
output = job_core_holdkillsig_sched.out
hold_kill_sig = 5
should_transfer_files   = YES
when_to_transfer_output = ON_EXIT_OR_EVICT
Notification = NEVER
wantcorefile = false
queue 1

