####################
##
## Test Condor command file
##
####################
universe        = standard
executable      = job_core_coredump.cndr.exe.$$(OPSYS).$$(ARCH)
error           = job_core_coredump-none_std.err
output          = job_core_coredump-none_std.out
log             = job_core_coredump-none_std.log
coresize        = 0
arguments       = Should not return core file -_condor_aggravate_bugs
priority        = 0
notification    = Never
queue
