####################
##
## Test Condor command file
##
####################

universe = standard
executable      = job_rsc_stat_std.cndr.exe.$$(OPSYS).$$(ARCH)
output          = job_rsc_stat_std.out
error           = job_rsc_stat_std.err
arguments	= x_data.in -_condor_aggravate_bugs

######
#
# This program uses ctime() which on some platforms will come up with
# a *very* wrong answer unless the "TZ" environment variable is set.
#
######
environment = TZ=CST6CDT;
log = job_rsc_stat_std.log
priority = 0

Notification = Never

queue 
