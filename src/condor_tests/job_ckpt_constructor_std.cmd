universe     = standard
executable   = job_ckpt_constructor_std.cndr.exe.$$(OPSYS).$$(ARCH)
output       = job_ckpt_constructor_std.out
error        = job_ckpt_constructor_std.err
log          = job_ckpt_constructor_std.log
arguments    = -_condor_aggravate_bugs
priority     = 0
notification = Never
queue
