universe = standard
executable = job_ckpt_gettimeofday_std.cndr.exe.$$(OPSYS).$$(ARCH)
output = job_ckpt_gettimeofday_std.out
error = job_ckpt_gettimeofday_std.err
log = job_ckpt_gettimeofday_std.log
arguments = 300 -_condor_aggravate_bugs
priority = 0

Notification = Never

queue
