####################
##
## Test Condor command file
##
####################

universe        = vanilla
executable      = job_core_coredump.exe
error           = job_core_coredump-yes_van.err
output          = job_core_coredump-yes_van.out
log             = job_core_coredump-yes_van.log
coresize        = 10000000000
arguments       = Should return core file -_condor_aggravate_bugs
priority        = 0
notification    = Never
queue
