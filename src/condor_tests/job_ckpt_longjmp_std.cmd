universe        = standard
executable      = job_ckpt_longjmp_std.cndr.exe.$$(OPSYS).$$(ARCH)
error           = job_ckpt_longjmp_std.err
output          = job_ckpt_longjmp_std.out
log             = job_ckpt_longjmp_std.log
arguments       = -_condor_D_CKPT -_condor_aggravate_bugs
priority        = 0
notification    = never
queue
