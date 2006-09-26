####################
##
## Test Condor command file
##
####################

universe = standard
executable      = job_ckpt_combo-sanity_std.cndr.exe.$$(OPSYS).$$(ARCH)
error           = job_ckpt_combo-sanity_std.err
output          = job_ckpt_combo-sanity_std.out
log		= job_ckpt_combo-sanity_std.log
arguments	= -_condor_D_CKPT -_condor_aggravate_bugs -_condor_D_FULLDEBUG
priority = -9


Notification = Never

queue
