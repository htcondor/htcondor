universe = standard
executable	= job_rsc_fgets_std.cndr.exe.$$(OPSYS).$$(ARCH)

output		= job_rsc_fgets_std.out
error		= job_rsc_fgets_std.err
log		= job_rsc_fgets_std.log
#requirements	= machine == "finch.cs.wisc.edu"

arguments = -_condor_aggravate_bugs

priority = 0

Notification = Never

queue
