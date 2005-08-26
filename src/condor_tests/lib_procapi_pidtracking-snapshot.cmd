universe = vanilla
executable = x_pid_tracking.pl
log = lib_procapi_pidtracking-snapshot.log
output = lib_procapi_pidtracking-snapshot.out
error = lib_procapi_pidtracking-snapshot.err
arguments = $$(OPSYS) 60 lib_procapi_pidtracking-snapshot
getenv=false
Notification = NEVER
queue

