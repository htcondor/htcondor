universe        = standard
executable      = job_ckpt_memory-file_std.cndr.exe.$$(OPSYS).$$(ARCH)
output          = job_ckpt_memory-file_std.out
error           = job_ckpt_memory-file_std.err
log             = job_ckpt_memory-file_std.log
priority        = 6
arguments       = -f x_job_ckpt_memory-file_std.t1 -s 10000 -o 25000 -_condor_aggravate_bugs
notification   = never
queue
