universe        = standard
executable      = job_ckpt_open-N-parallel_std.cndr.exe.$$(OPSYS).$$(ARCH)
error           = job_ckpt_open-N-parallel_std.err
output          = job_ckpt_open-N-parallel_std.out
log             = job_ckpt_open-N-parallel_std.log
arguments       = x_data-1.in x_data-2.in x_data-3.in x_data-4.in x_data-5.in x_data-6.in x_data-7.in x_data-8.in -_condor_aggravate_bugs
priority        = 0
notification    = never
queue
