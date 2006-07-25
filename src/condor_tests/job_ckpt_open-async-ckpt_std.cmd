universe        = standard
executable      = job_ckpt_open-async-ckpt_std.cndr.exe.$$(OPSYS).$$(ARCH)
output          = job_ckpt_open-async-ckpt_std.out
error           = job_ckpt_open-async-ckpt_std.err
log             = job_ckpt_open-async-ckpt_std.log
arguments       = -_condor_aggravate_bugs
priority        = 7
notification    = Never
queue
