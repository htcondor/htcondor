universe = standard
executable	= job_rsc_ftell_std.cndr.exe.$$(OPSYS).$$(ARCH)
output		= job_rsc_ftell_std.out
error		= job_rsc_ftell_std.err
log		= job_rsc_ftell_std.log
arguments	= -_condor_aggravate_bugs
priority = 0

Notification = Never

queue
