universe = scheduler
executable = x_perf_busy.pl
arguments = 20
output = perf_busy_lynn_100_100_500_sched.out
error = perf_busy_lynn_100_100_500_sched.err
log = perf_busy_lynn_100_100_500_sched.log
periodic_remove = (CurrentTime - QDate) > (24 * 60 * 60)
should_transfer_files   = YES
when_to_transfer_output = ON_EXIT_OR_EVICT
Notification = NEVER
queue 300

