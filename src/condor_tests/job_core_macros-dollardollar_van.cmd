universe   = vanilla
executable = x_print-args-and-env.pl
output = job_core_macros-dollardollar_van.out
error = job_core_macros-dollardollar_van.err
log = job_core_macros-dollardollar_van.log
env = MEMORY=$$(memory);MACHINE=$$(machine)
arguments = $$(memory) $$(machine)

Notification = NEVER
queue

