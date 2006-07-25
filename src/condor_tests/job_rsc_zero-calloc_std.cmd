universe        = standard
executable      = job_rsc_zero-calloc_std.cndr.exe.$$(OPSYS).$$(ARCH)
output          = job_rsc_zero-calloc_std.out
error           = job_rsc_zero-calloc_std.err
log             = job_rsc_zero-calloc_std.log
arguments       = -_condor_aggravate_bugs
priority        = 0
notification    = never
queue
