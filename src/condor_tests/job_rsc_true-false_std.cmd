####################
##
## Test Condor command file
##
####################

universe = standard
executable      = job_rsc_true-false_std.cndr.exe.$$(OPSYS).$$(ARCH)
output          = job_rsc_true-false_std.out
error           = job_rsc_true-false_std.err
arguments = /tmp/job_rsc_true-false_std.17612
######
#
#
######
want_remote_io = false
environment = TZ=CST6CDT;
log = job_rsc_true-false_std.log
#priority = 0

Notification = Never

queue 
