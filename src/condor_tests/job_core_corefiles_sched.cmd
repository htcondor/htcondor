universe   = scheduler
executable = x_dumpcore.exe
output = job_core_corefiles_sched.out
error = job_core_corefiles_sched.err
log = job_core_corefiles_sched.log
coresize = 600000
should_transfer_files   = YES
when_to_transfer_output = ON_EXIT_OR_EVICT
Notification = NEVER
queue

