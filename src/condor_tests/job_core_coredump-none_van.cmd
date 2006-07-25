####################
##
## Test Condor command file
##
####################

universe        = vanilla
executable      = job_core_coredump.exe
error           = job_core_coredump-none_van.err
output          = job_core_coredump-none_van.out
log             = job_core_coredump-none_van.log
coresize        = 0
arguments       = Should not return core file -_condor_aggravate_bugs
priority        = 0
notification    = Never
queue
