####################
##
## Test Condor command file
##
####################

executable      = job_ckpt_f-registers.remote.$$(OPSYS).$$(ARCH)
error           = job_ckpt_f-registers.err
output          = job_ckpt_f-registers.out
log		= job_ckpt_f-registers.log
arguments = -_condor_aggravate_bugs



Notification = Never

queue
