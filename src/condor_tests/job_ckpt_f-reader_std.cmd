universe	= standard
executable      = job_ckpt_f-reader_std.cndr.exe.$$(OPSYS).$$(ARCH)
input		= job_ckpt_f-reader_std.in
output          = job_ckpt_f-reader_std.out
error           = job_ckpt_f-reader_std.err
log		= job_ckpt_f-reader_std.log
arguments = -_condor_aggravate_bugs
Notification = Never
queue
