####################
##
## Test Condor command file
##
####################

universe	= standard
executable      = job_rsc_f-direct-write_std.cndr.exe.$$(OPSYS).$$(ARCH)
error           = job_rsc_f-direct-write_std.err
output          = job_rsc_f-direct-write_std.out
log		= job_rsc_f-direct-write_std.log
arguments = -_condor_aggravate_bugs


Notification = Never

queue
