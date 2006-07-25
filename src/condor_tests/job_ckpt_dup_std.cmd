####################
##
## Test Condor command file
##
####################

universe = standard
executable      = job_ckpt_dup_std.cndr.exe.$$(OPSYS).$$(ARCH)
error           = job_ckpt_dup_std.err
output          = job_ckpt_dup_std.out
arguments	= x_data.in -_condor_aggravate_bugs
log		= job_ckpt_dup_std.log
priority = 0

Notification = Never

queue
