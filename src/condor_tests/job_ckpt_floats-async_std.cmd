universe        = standard
executable      = job_ckpt_floats-async_std.cndr.exe.$$(OPSYS).$$(ARCH)
output          = job_ckpt_floats-async_std.out
error           = job_ckpt_floats-async_std.err
log             = job_ckpt_floats-async_std.log
arguments       = -_condor_aggravate_bugs 45
priority        = 10
notification    = never
queue
