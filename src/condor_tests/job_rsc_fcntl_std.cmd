universe = standard
Executable = job_rsc_fcntl_std.cndr.exe.$$(OPSYS).$$(ARCH)
output = job_rsc_fcntl_std.out
error = job_rsc_fcntl_std.err
log = job_rsc_fcntl_std.log
arguments = -_condor_aggravate_bugs
priority = 0

Notification = Never

queue 
