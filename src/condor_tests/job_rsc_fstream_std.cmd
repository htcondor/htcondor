####################
##
## Test Condor command file
##
####################

universe = standard
executable      = job_rsc_fstream_std.cndr.exe.$$(OPSYS).$$(ARCH)
error           = job_rsc_fstream_std.err
output          = job_rsc_fstream_std.out
log		= job_rsc_fstream_std.log
arguments = -_condor_aggravate_bugs

priority = 0

Notification = Never

queue
