####################
##
## Test Condor command file
##
####################

executable      = job_ckpt_f-floats_std.cndr.$$(OPSYS).$$(ARCH)
error           = job_ckpt_f-floats_std.err
output          = job_ckpt_f-floats_std.out
log		= job_ckpt_f-floats_std.log
arguments = -_condor_aggravate_bugs


Notification = Never

queue
