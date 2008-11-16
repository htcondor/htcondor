####################
##
## Test Condor command file
##
####################

universe	= standard
executable      = job_ckpt_f-integers_std.cndr.exe.$$(OPSYS).$$(ARCH)
error           = job_ckpt_f-integers_std.err
output          = job_ckpt_f-integers_std.out
log		= job_ckpt_f-integers_std.log
arguments = -_condor_aggravate_bugs



Notification = Never

queue
