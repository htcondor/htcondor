universe = standard
executable = job_rsc_all-syscalls_std.cndr.exe.$$(OPSYS).$$(ARCH)
output = job_rsc_all-syscalls_std.out
error = job_rsc_all-syscalls_std.err
log = job_rsc_all-syscalls_std.log
arguments = -_condor_aggravate_bugs
#Arguments = -_condor_debug_wait -_condor_aggravate_bugs

priority = 0

Notification = Never

queue
