universe = vanilla
executable = x_pid_tracking.pl
log = lib_procapi_pidtracking-byenv.log
output = lib_procapi_pidtracking-byenv.out
error = lib_procapi_pidtracking-byenv.err
arguments = $$(OPSYS)  3 lib_procapi_pidtracking-byenv
getenv=false
Notification = NEVER
queue

