universe   = local
executable = x_sleep.pl
log = job_core_perremove-true_local.log
output = job_core_perremove-true_local.out
error = job_core_perremove-true_local.err
Notification = NEVER
##
## The PERIODIC_EXPR_INTERVAL is set to 15 for tests so we need
## to make sure we sleep for just a little bit more than that
##
arguments  = 20
periodic_remove = (CurrentTime - QDate) > 2
queue

