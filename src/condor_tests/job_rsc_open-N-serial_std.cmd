universe        = standard
executable      = job_rsc_open-N-serial_std.remote.$$(OPSYS).$$(ARCH)
output          = job_rsc_open-N-serial_std.out
error           = job_rsc_open-N-serial_std.error
log             = job_rsc_open-N-serial_std.log
arguments       = 1000 -_condor_aggravate_bugs
priority        = 0
notification    = never
queue
