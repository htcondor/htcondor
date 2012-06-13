universe        = standard
executable      = job_ckpt_io-buffer-async_std.cndr.exe.$$(OPSYS).$$(ARCH)
output          = job_ckpt_stack_with_ckpt_server_std.out
error           = job_ckpt_stack_with_ckpt_server_std.err
log             = job_ckpt_stack_with_ckpt_server_std.log
arguments       = x_job_ckpt_io-buffer-async_std.random x_job_ckpt_io-buffer-async_std.linear -_condor_aggravate_bugs
priority        = 0
notification    = never
queue
