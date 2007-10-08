universe = vanilla
executable = x_cpu_tracking.pl
log = lib_procapi_cputracking-snapshot.log
output = lib_procapi_cputracking-snapshot.out
error = lib_procapi_cputracking-snapshot.err
arguments = $$(OPSYS) 120 lib_procapi_cputracking-snapshot
getenv=true
Notification = NEVER
queue

