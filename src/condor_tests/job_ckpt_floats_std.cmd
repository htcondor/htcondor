####################
##
## Test Condor command file
##
####################

universe = standard
executable      = job_ckpt_floats_std.cndr.exe.$$(OPSYS).$$(ARCH)
error           = job_ckpt_floats_std.err
output          = job_ckpt_floats_std.out
log		= job_ckpt_floats_std.log
arguments = -_condor_aggravate_bugs
priority = 0

Notification = Never

queue
