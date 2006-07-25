####################
##
## Test Condor command file
##
####################

universe        = standard
executable      = job_core_coredump.cndr.exe.$$(OPSYS).$$(ARCH)
error           = job_core_coredump-yes_std.err
output          = job_core_coredump-yes_std.out
log             = job_core_coredump-yes_std.log
coresize        = 10000000000
arguments       = Should return core file -_condor_aggravate_bugs
priority        = 0
notification    = Never
queue
