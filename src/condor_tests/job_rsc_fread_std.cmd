universe = standard
executable	= job_rsc_fread_std.cndr.exe.$$(OPSYS).$$(ARCH)

output		= job_rsc_fread_std.out
error		= job_rsc_fread_std.err
log		= job_rsc_fread_std.log
arguments = -_condor_aggravate_bugs

priority = 0

Notification = Never

queue
