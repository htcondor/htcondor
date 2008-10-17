universe	= standard
executable      = job_ckpt_io-async_std.cndr.exe.$$(OPSYS).$$(ARCH)
output          = job_ckpt_io-async_std.out
error           = job_ckpt_io-async_std.err
log             = job_ckpt_io-async_std.log
arguments       = 180 -_condor_aggravate_bugs
priority        = 8
notification    = never
queue
