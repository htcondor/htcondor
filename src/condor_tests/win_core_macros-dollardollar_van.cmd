universe   = vanilla
executable = x_print-args-and-env.pl
output = win_core_macros-dollardollar_van.out
error = win_core_macros-dollardollar_van.err
log = win_core_macros-dollardollar_van.log
env = MEMORY=$$(memory)|MACHINE=$$(machine)|WONTBE=$$(wontbethere:/be/here/now)
# out put file has herader line making these $lines[1], $lines[2] ...
arguments = $$(memory) $$(machine) $$(wontbethere:/be/here/now)

Notification = NEVER
queue

