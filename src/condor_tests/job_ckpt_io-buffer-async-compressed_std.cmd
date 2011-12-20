universe        = standard
executable      = job_ckpt_io-buffer-async-compressed_std.cndr.exe.$$(OPSYS).$$(ARCH)
output          = job_ckpt_io-buffer-async-compressed_std.out
error           = job_ckpt_io-buffer-async-compressed_std.err
log             = job_ckpt_io-buffer-async-compressed_std.log
arguments       = x_job_ckpt_io-buffer-async-compressed_std.random x_job_ckpt_io-buffer-async-compressed_std.linear -_condor_aggravate_bugs
compress_files  = x_job_ckpt_io-buffer-async-compressed_std.random x_job_ckpt_io-buffer-async-compressed_std.linear -_condor_aggravate_bugs
priority        = 0
notification    = never
queue
