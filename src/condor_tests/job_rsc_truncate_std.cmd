universe = standard
executable = job_rsc_truncate_std.cndr.exe.$$(OPSYS).$$(ARCH)
output = job_rsc_truncate_std.out
error = job_rsc_truncate_std.err
log = job_rsc_truncate_std.log
arguments = job_rsc_truncate_std.temp$(PROCESS) -_condor_aggravate_bugs
buffer_size = 100
priority = 0

Notification = Never

queue
