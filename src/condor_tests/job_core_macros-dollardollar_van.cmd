universe   = vanilla
executable = x_print-args-and-env.pl
output = job_core_macros-dollardollar_van.out
error = job_core_macros-dollardollar_van.err
log = job_core_macros-dollardollar_van.log
env = MEMORY=$$(memory);MACHINE=$$(machine);WONTBE=$$(wontbethere:/be/here/now);
# out put file has herader line making these $lines[1], $lines[2] ...
arguments = MEMORY:$$(memory) MACHINE:$$(machine) WONTBE:$$(wontbethere:/be/here/now)

Notification = NEVER
queue

