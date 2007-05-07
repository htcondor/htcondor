universe   = local
executable = x_sleep.pl
log = job_core_perhold-false_local.log
output = job_core_perhold-false_local.out
error = job_core_perhold-false_local.err
hold	= false
Notification = NEVER
##
## The PERIODIC_EXPR_INTERVAL is set to 15 for tests so we need
## to make sure we sleep for just a little bit more than that
##
arguments  = 20
periodic_hold = (CurrentTime - QDate) < 0
queue

