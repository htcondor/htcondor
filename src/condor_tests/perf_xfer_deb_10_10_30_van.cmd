universe = vanilla
executable = x_perf_transfer_remote.pl
arguments = backdata
transfer_input_files = transfers/data
output = perf_xfer_deb_10_10_30_van.out
error = perf_xfer_deb_10_10_30_van.err
log = perf_xfer_deb_10_10_30_van.log
periodic_remove = (CurrentTime - QDate) > (24 * 60 * 60)
should_transfer_files   = YES
when_to_transfer_output = ON_EXIT_OR_EVICT
Notification = NEVER
queue 30

