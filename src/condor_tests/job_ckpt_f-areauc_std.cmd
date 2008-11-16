universe	= standard
executable      = job_ckpt_f-areauc_std.cndr.exe.$$(OPSYS).$$(ARCH)
error           = job_ckpt_f-areauc_std.err
output          = job_ckpt_f-areauc_std.out
log             = job_ckpt_f-areauc_std.log
arguments       = -_condor_aggravate_bugs
Notification    = Never
queue
