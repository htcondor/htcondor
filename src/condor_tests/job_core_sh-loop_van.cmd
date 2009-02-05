universe        = vanilla
executable      = job_core_sh-loop_van.sh
output          = job_core_sh-loop_van.out
error           = job_core_sh-loop_van.err
log             = job_core_sh-loop_van.log
arguments       = 12 -_condor_aggravate_bugs
priority        = 0
should_transfer_files = YES
when_to_transfer_output = ON_EXIT
Notification = Never
queue
