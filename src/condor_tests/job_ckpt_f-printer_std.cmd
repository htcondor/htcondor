####################
##
## Test Condor command file
##
####################

executable      = job_ckpt_f-printer_std.cndr.exe.$$(OPSYS).$$(ARCH)
error           = job_ckpt_f-printer_std.err
output          = job_ckpt_f-printer_std.out
log		= job_ckpt_f-printer_std.log
arguments = -_condor_aggravate_bugs



Notification = Never

queue
